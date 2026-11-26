/*
 * dplay.c: DirectPlay for the game (port code, not from the original): DPLAYX.dll's two entry
 * points and the IDirectPlay / IDirectPlay3A objects TM2.EXE uses (docs/NETWORK.md section 1),
 * on Roadkill's own provider (net_session.c; not Microsoft's wire protocol).
 *
 * DPLAYX.dll is imported by ordinal (the runtime names the slots "DPLAYX.dll!#n"):
 *   #2 DirectPlayEnumerateA(callback, context), slot 0x00c907d4 (thunk 0x004d1902): one service
 *      provider, "Internet TCP/IP Connection For DirectPlay" with DPSPGUID_TCPIP (the game only
 *      lists providers whose name has "Modem", "IPX" or "TCP" in it), when the platform has
 *      sockets; none otherwise (the Multi Player screen then says so and goes back).
 *   #1 DirectPlayCreate(GUID*, IDirectPlay** out, outer), slot 0x00c907d8 (thunk 0x004d1908;
 *      en7_a.c calls it "DirectSoundCreate"): an IDirectPlay (v1); its QueryInterface gives the
 *      IDirectPlay3A every session call goes through. Both share one state; each interface keeps
 *      its own count (the game releases both). Nothing opens a socket until EnumSessions or Open
 *      (the boot probe creates and releases an object for every provider).
 *
 * What the game gets, by the rules of DirectX 5's DPLAYX as far as the game relies on them:
 *   - EnumSessions answers at once from the session cache (net_discovery.c), then calls back
 *     once more with DPESC_TIMEDOUT; it never waits its timeout, never fails, never shows a
 *     dialog (the real TCP/IP provider asked for an address and minimized the game).
 *     DPENUMSESSIONS_AVAILABLE hides full sessions and those with new players disabled.
 *     Password protected sessions are listed (DPSESSION_PASSWORDREQUIRED set); Open checks the
 *     password.
 *   - Receive builds the DPSYS_* system messages in the caller's buffer, pointers into that same
 *     buffer (the game follows DPMSG_SETPLAYERORGROUPDATA's lpData as a game address); every
 *     message fits the game's 0x400 bytes. Nothing from the wire is ever used as a pointer.
 *   - Send never answers DPERR_BUSY (the game would spin).
 * Methods the game never calls answer DPERR_UNSUPPORTED with a log line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/dx_com.h"
#include "host/net/net_discovery.h"
#include "host/net/net_log.h"
#include "host/net/net_session.h"
#include "host/net/net_sock.h"
#include "host/win32_host.h"

/* DPSPGUID_TCPIP {36E95EE0-8577-11CF-960C-0080C7534E82} */
static const uint8_t SP_GUID[16] = {0xe0, 0x5e, 0xe9, 0x36, 0x77, 0x85, 0xcf, 0x11,
                                    0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82};
static const char SP_NAME[] = "Internet TCP/IP Connection For DirectPlay";

#define IID_IDIRECTPLAY   0x5454e9a0u
#define IID_IDIRECTPLAY2A 0x9d460580u
#define IID_IDIRECTPLAY3A 0x133efe41u
#define CLASS_E_NOAGGREGATION 0x80040110u

/* DPENUMSESSIONS_*, DPENUMPLAYERS_*, DPRECEIVE_*, DPOPEN_* */
#define ES_AVAILABLE 0x01u
#define ES_ALL       0x02u
#define EP_LOCAL     0x08u
#define EP_REMOTE    0x10u
#define EP_SESSION   0x80u
#define RC_ALL       0x01u
#define RC_PEEK      0x08u
#define OPEN_JOIN    0x01u
#define OPEN_CREATE  0x02u
#define DPESC_TIMEDOUT 1u
#define DPPLAYERTYPE_PLAYER 1u

typedef struct DpState {
    int refs;                /* interface objects alive */
    DxObj* v1;
    DxObj* v3;
    int in_session;          /* this object opened the session */
} DpState;

static DxIface I_DP1 = {"IDirectPlay", 0, {IID_IDIRECTPLAY}};
static DxIface I_DP3 = {"IDirectPlay3A", 0, {IID_IDIRECTPLAY3A, IID_IDIRECTPLAY2A}};

/* ---- game memory: bounded copies, never a host pointer from a game value ---- */

static int gok(uint32_t a, uint32_t n) { return a != 0 && a < RT_SIZE && n <= RT_SIZE - a; }

static void gstr(uint32_t a, char* out, int cap)
{
    int i = 0;
    if (a && a < RT_SIZE)
        for (; i < cap - 1 && a + (uint32_t)i < RT_SIZE; i++) {
            char c = (char)rt_r8(a + (uint32_t)i);
            if (!c) break;
            out[i] = c;
        }
    out[i] = 0;
}

static int gread(uint32_t a, void* out, uint32_t n)
{
    if (!n) return 0;
    if (!gok(a, n)) return -1;
    memcpy(out, rt_ptr(a), n);
    return 0;
}

static int gwrite(uint32_t a, const void* in, uint32_t n)
{
    if (!n) return 0;
    if (!gok(a, n)) return -1;
    memcpy(rt_ptr(a), in, n);
    return 0;
}

static void put32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

/* ---- the objects ---- */

static DpState* state_of(const uint32_t* a, int n)
{
    DxObj* o = dx_obj_get(dx_arg(a, n, 0), NULL);
    if (!o || (o->iface != &I_DP1 && o->iface != &I_DP3)) return NULL;
    return o->host;
}

static void state_unref(DpState* s)
{
    if (--s->refs > 0) return;
    if (s->in_session) ns_close();
    free(s);
}

static void v1_destroy(DxObj* o) { DpState* s = o->host; s->v1 = NULL; state_unref(s); }
static void v3_destroy(DxObj* o) { DpState* s = o->host; s->v3 = NULL; state_unref(s); }

static uint32_t m_qi(const uint32_t* a, int n)
{
    DxObj* o = dx_obj_get(dx_arg(a, n, 0), NULL);
    uint32_t iid = dx_arg(a, n, 1), out = dx_arg(a, n, 2);
    if (!o || !out || !gok(out, 4)) return DX_E_POINTER;
    DpState* s = o->host;
    uint32_t d1 = iid && gok(iid, 4) ? rt_r32(iid) : 0;
    DxObj* r = NULL;
    if (d1 == IID_IDIRECTPLAY3A || d1 == IID_IDIRECTPLAY2A) {
        if (!s->v3) {
            s->v3 = dx_obj_new(&I_DP3, s, v3_destroy);
            if (s->v3) s->refs++;
            r = s->v3;
        } else {
            r = s->v3;
            dx_obj_addref(r);
        }
    } else if (d1 == IID_IDIRECTPLAY) {
        if (!s->v1) {
            s->v1 = dx_obj_new(&I_DP1, s, v1_destroy);
            if (s->v1) s->refs++;
            r = s->v1;
        } else {
            r = s->v1;
            dx_obj_addref(r);
        }
    } else if (d1 == 0) {   /* IID_IUnknown */
        r = o;
        dx_obj_addref(r);
    }
    if (!r) {
        dx_log_once("%s::QueryInterface: no interface %08x", o->iface->name, d1);
        rt_w32(out, 0);
        return DX_E_NOINTERFACE;
    }
    rt_w32(out, r->addr);
    return DX_OK;
}

/* ---- methods ---- */

static uint32_t m_close(const uint32_t* a, int n)
{
    DpState* s = state_of(a, n);
    if (!s) return DPERR_INVALIDOBJECT;
    if (s->in_session) {
        ns_close();
        s->in_session = 0;
    }
    return DP_OK;
}

/* IDirectPlay::EnableNewPlayers(BOOL) */
static uint32_t m_enable_new_players(const uint32_t* a, int n)
{
    DpState* s = state_of(a, n);
    if (!s) return DPERR_INVALIDOBJECT;
    ns_enable_new_players(dx_arg(a, n, 1) != 0);
    return DP_OK;
}

static uint32_t m_initialize(const uint32_t* a, int n)
{
    (void)a; (void)n;
    return DPERR_ALREADYINITIALIZED;
}

/* CreatePlayer(lpidPlayer, lpPlayerName, hEvent, lpData, dwDataSize, dwFlags) */
static uint32_t m_create_player(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t pid = dx_arg(a, n, 1), name = dx_arg(a, n, 2), data = dx_arg(a, n, 4), size = dx_arg(a, n, 5);
    uint32_t flags = dx_arg(a, n, 6);
    if (!gok(pid, 4) || size > NW_MAX_PDATA || (size && !gok(data, size))) return DPERR_INVALIDPARAMS;
    char sname[NW_MAX_NAME + 1] = "", lname[NW_MAX_NAME + 1] = "";
    if (name && gok(name, 16)) {
        gstr(rt_r32(name + 8), sname, sizeof sname);
        gstr(rt_r32(name + 12), lname, sizeof lname);
    }
    uint8_t buf[NW_MAX_PDATA];
    if (size) gread(data, buf, size);
    uint32_t id = 0;
    uint32_t hr = ns_create_player(sname, lname, buf, (int)size, flags, &id);
    if (hr == DP_OK) rt_w32(pid, id);
    return hr;
}

static uint32_t m_destroy_player(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    return ns_destroy_player(dx_arg(a, n, 1));
}

/* a DPNAME and its two strings at f (0x10 + 2 x 0x80 bytes) */
static uint32_t put_name(uint32_t f, const char* sname, const char* lname)
{
    uint32_t s = f + 0x10, l = f + 0x10 + 0x80;
    rt_w32(f, 0x10);
    rt_w32(f + 4, 0);
    rt_w32(f + 8, s);
    rt_w32(f + 12, l);
    gwrite(s, sname, (uint32_t)strlen(sname) + 1);
    gwrite(l, lname, (uint32_t)strlen(lname) + 1);
    return f;
}

/* EnumPlayers(lpguidInstance, lpEnumPlayersCallback2, lpContext, dwFlags) */
static uint32_t m_enum_players(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t guid = dx_arg(a, n, 1), cb = dx_arg(a, n, 2), ctx = dx_arg(a, n, 3), flags = dx_arg(a, n, 4);
    if (!cb) return DPERR_INVALIDPARAMS;
    NsPlayer list[NW_MAX_PLAYERS], mine[NW_MAX_PLAYERS];
    int np = 0;
    if (flags & EP_SESSION) {
        /* a session we are not in (the join screen): the names its host announced */
        uint8_t g[16];
        NdEntry e;
        if (!gok(guid, 16) || gread(guid, g, 16) != 0) return DPERR_INVALIDPARAMS;
        if (nd_find(g, &e, net_now_ms()) != 0) return DPERR_NOSESSIONS;
        for (int i = 0; i < e.s.nplayers && np < NW_MAX_PLAYERS; i++) {
            memset(&mine[np], 0, sizeof mine[np]);
            mine[np].id = e.s.players[i].id;
            memcpy(mine[np].sname, e.s.players[i].sname, sizeof mine[np].sname);
            memcpy(mine[np].lname, e.s.players[i].lname, sizeof mine[np].lname);
            np++;
        }
    } else {
        if (!ns_in_session()) return DPERR_NOSESSIONS;
        np = ns_players(list, NW_MAX_PLAYERS);
        int k = 0;
        for (int i = 0; i < np; i++) {
            if ((flags & EP_LOCAL) && !list[i].local) continue;
            if ((flags & EP_REMOTE) && list[i].local) continue;
            mine[k++] = list[i];
        }
        np = k;
    }
    /* the callback may block on a game mutex and let another game thread in: everything it
     * reads lives in this thread's own frame */
    static int last_np = -1;
    static uint32_t last_flags;
    if (np != last_np || flags != last_flags) {
        net_log("EnumPlayers(flags %x): %d players", flags, np);
        last_np = np;
        last_flags = flags;
    }
    uint32_t f = rt_frame_push(0x120);
    for (int i = 0; i < np; i++) {
        put_name(f, mine[i].sname, mine[i].lname);
        uint32_t w[5] = {mine[i].id, DPPLAYERTYPE_PLAYER, f, (flags & EP_SESSION) ? 0 : mine[i].local ? EP_LOCAL : EP_REMOTE, ctx};
        if (!dx_call_cb(cb, w, 5)) break;
    }
    rt_frame_pop(0x120);
    return DP_OK;
}

/* a DPSESSIONDESC2 (0x50) and its name (0x80) at f */
static void put_desc(uint32_t f, const NwSessionDesc* d)
{
    memset(rt_ptr(f), 0, 0x50);
    rt_w32(f, 0x50);
    rt_w32(f + 4, d->flags);
    gwrite(f + 8, d->guid, 16);
    gwrite(f + 0x18, d->app, 16);
    rt_w32(f + 0x28, d->max_players);
    rt_w32(f + 0x2c, d->cur_players);
    rt_w32(f + 0x30, f + 0x50);
    rt_w32(f + 0x34, 0);   /* lpszPassword: not given out */
    for (int i = 0; i < 4; i++) rt_w32(f + 0x40 + 4u * (uint32_t)i, d->user[i]);
    gwrite(f + 0x50, d->name, (uint32_t)strlen(d->name) + 1);
}

/* EnumSessions(lpsd, dwTimeout, lpEnumSessionsCallback2, lpContext, dwFlags) */
static uint32_t m_enum_sessions(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t sd = dx_arg(a, n, 1), timeout = dx_arg(a, n, 2), cb = dx_arg(a, n, 3), ctx = dx_arg(a, n, 4);
    uint32_t flags = dx_arg(a, n, 5);
    if (!cb) return DPERR_INVALIDPARAMS;
    uint8_t app[16], zero[16] = {0};
    int filter = sd && gok(sd, 0x50) && gread(sd + 0x18, app, 16) == 0 && memcmp(app, zero, 16) != 0;
    /* the callback may block and let another game thread enumerate too: a list of our own */
    NdEntry* list = malloc(sizeof *list * ND_MAX);
    int ne = 0;
    if (list && ns_start() == 0) {
        uint32_t now = net_now_ms();
        nd_want(now);
        ne = nd_list(list, ND_MAX, filter ? app : NULL, now);
    }
    uint32_t f = rt_frame_push(0x50 + 0x80 + 4);
    uint32_t tw = f + 0xd0;
    rt_w32(tw, timeout);
    int go = 1;
    for (int i = 0; i < ne && go; i++) {
        const NwSessionDesc* d = &list[i].s.d;
        if (!(flags & ES_ALL) && ((d->flags & NS_NEWPLAYERSDISABLED) || d->cur_players >= d->max_players)) continue;
        put_desc(f, d);
        uint32_t w[4] = {f, tw, 0, ctx};
        go = dx_call_cb(cb, w, 4) != 0;
    }
    if (go) {
        uint32_t w[4] = {0, tw, DPESC_TIMEDOUT, ctx};
        dx_call_cb(cb, w, 4);
    }
    rt_frame_pop(0x50 + 0x80 + 4);
    free(list);
    return DP_OK;
}

/* GetCaps(lpDPCaps, dwFlags): DPCAPS is 0x28 bytes; the game never sets dwSize (en7_h.c:98) and
 * ignores the answer: as DPLAYX, a wrong dwSize is DPERR_INVALIDPARAMS and nothing is written */
static uint32_t m_get_caps(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t c = dx_arg(a, n, 1);
    if (!gok(c, 0x28) || rt_r32(c) != 0x28) return DPERR_INVALIDPARAMS;
    rt_w32(c + 4, 0x00000004u);           /* dwFlags: DPCAPS_GUARANTEEDSUPPORTED (... OPTIMIZED etc. not claimed) */
    rt_w32(c + 8, NW_MAX_GAME);           /* dwMaxBufferSize */
    rt_w32(c + 12, 0);                    /* dwMaxQueueSize */
    rt_w32(c + 16, NW_MAX_PLAYERS);       /* dwMaxPlayers */
    rt_w32(c + 20, 0);                    /* dwHundredBaud */
    rt_w32(c + 24, 50);                   /* dwLatency (ms) */
    rt_w32(c + 28, 1);                    /* dwMaxLocalPlayers */
    rt_w32(c + 32, NW_HDR);               /* dwHeaderLength */
    rt_w32(c + 36, 10000);                /* dwTimeout */
    return DP_OK;
}

static uint32_t m_get_message_count(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t pc = dx_arg(a, n, 2);
    if (!gok(pc, 4)) return DPERR_INVALIDPARAMS;
    rt_w32(pc, (uint32_t)ns_queued());
    return DP_OK;
}

/* GetPlayerData(idPlayer, lpData, lpdwDataSize, dwFlags) */
static uint32_t m_get_player_data(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t data = dx_arg(a, n, 2), psize = dx_arg(a, n, 3);
    if (!gok(psize, 4)) return DPERR_INVALIDPARAMS;
    NsPlayer p;
    uint32_t hr = ns_get_player(dx_arg(a, n, 1), &p);
    if (hr) return hr;
    if (!data || rt_r32(psize) < p.n) { rt_w32(psize, p.n); return DPERR_BUFFERTOOSMALL; }
    if (gwrite(data, p.data, p.n) != 0) return DPERR_INVALIDPARAMS;
    rt_w32(psize, p.n);
    return DP_OK;
}

/* GetPlayerName(idPlayer, lpData, lpdwDataSize): a DPNAME and its strings */
static uint32_t m_get_player_name(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t data = dx_arg(a, n, 2), psize = dx_arg(a, n, 3);
    if (!gok(psize, 4)) return DPERR_INVALIDPARAMS;
    NsPlayer p;
    uint32_t hr = ns_get_player(dx_arg(a, n, 1), &p);
    if (hr) return hr;
    uint32_t sl = (uint32_t)strlen(p.sname) + 1, ll = (uint32_t)strlen(p.lname) + 1, need = 0x10 + sl + ll;
    if (!data || rt_r32(psize) < need) { rt_w32(psize, need); return DPERR_BUFFERTOOSMALL; }
    if (!gok(data, need)) return DPERR_INVALIDPARAMS;
    rt_w32(data, 0x10);
    rt_w32(data + 4, 0);
    rt_w32(data + 8, data + 0x10);
    rt_w32(data + 12, data + 0x10 + sl);
    gwrite(data + 0x10, p.sname, sl);
    gwrite(data + 0x10 + sl, p.lname, ll);
    rt_w32(psize, need);
    return DP_OK;
}

/* GetSessionDesc(lpData, lpdwDataSize) */
static uint32_t m_get_session_desc(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t data = dx_arg(a, n, 1), psize = dx_arg(a, n, 2);
    if (!gok(psize, 4)) return DPERR_INVALIDPARAMS;
    NwSessionDesc d;
    if (ns_session_desc(&d) != 0) return DPERR_NOCONNECTION;
    uint32_t need = 0x50 + (uint32_t)strlen(d.name) + 1;
    if (!data || rt_r32(psize) < need) { rt_w32(psize, need); return DPERR_BUFFERTOOSMALL; }
    if (!gok(data, need)) return DPERR_INVALIDPARAMS;
    put_desc(data, &d);
    rt_w32(psize, need);
    return DP_OK;
}

/* Open(lpsd, dwFlags) */
static uint32_t m_open(const uint32_t* a, int n)
{
    DpState* s = state_of(a, n);
    if (!s) return DPERR_INVALIDOBJECT;
    uint32_t sd = dx_arg(a, n, 1), flags = dx_arg(a, n, 2);
    if (!gok(sd, 0x50) || rt_r32(sd) != 0x50) return DPERR_INVALIDPARAMS;
    char password[NW_MAX_NAME + 1];
    gstr(rt_r32(sd + 0x34), password, sizeof password);
    uint32_t hr;
    if (flags & OPEN_CREATE) {
        NwSessionDesc d;
        memset(&d, 0, sizeof d);
        d.flags = rt_r32(sd + 4);
        gread(sd + 0x18, d.app, 16);
        d.max_players = rt_r32(sd + 0x28);
        gstr(rt_r32(sd + 0x30), d.name, sizeof d.name);
        for (int i = 0; i < 4; i++) d.user[i] = rt_r32(sd + 0x40 + 4u * (uint32_t)i);
        hr = ns_host(&d, password);
    } else if (flags & OPEN_JOIN) {
        uint8_t g[16];
        gread(sd + 8, g, 16);
        /* waits for the host: the runtime's host lock is given up meanwhile, as Sleep does */
        w32_block_begin();
        hr = ns_join(g, password, NULL);
        w32_block_end();
    } else return DPERR_INVALIDFLAGS;
    if (hr == DP_OK) s->in_session = 1;
    return hr;
}

/* builds the message ev for the buffer at game address buf into out (host memory); returns its size */
static uint32_t build(const NsEvent* ev, uint32_t buf, uint8_t* out)
{
    uint32_t sl = (uint32_t)strlen(ev->sname) + 1, ll = (uint32_t)strlen(ev->lname) + 1;
    switch (ev->kind) {
    case NS_EV_GAME:
        memcpy(out, ev->data, ev->n);
        return ev->n;
    case NS_EV_CREATE: {   /* DPMSG_CREATEPLAYERORGROUP, 0x30, then data, short and long name */
        memset(out, 0, 0x30);
        uint32_t d = 0x30, s = d + ev->n, l = s + sl;
        put32(out, 0x0003);
        put32(out + 4, DPPLAYERTYPE_PLAYER);
        put32(out + 8, ev->id);
        put32(out + 0xc, ev->cur_players);
        put32(out + 0x10, ev->n ? buf + d : 0);
        put32(out + 0x14, ev->n);
        put32(out + 0x18, 0x10);
        put32(out + 0x20, buf + s);
        put32(out + 0x24, buf + l);
        memcpy(out + d, ev->data, ev->n);
        memcpy(out + s, ev->sname, sl);
        memcpy(out + l, ev->lname, ll);
        return l + ll;
    }
    case NS_EV_DESTROY: {   /* DPMSG_DESTROYPLAYERORGROUP, 0x34, then the remote data and the names */
        memset(out, 0, 0x34);
        uint32_t d = 0x34, s = d + ev->n, l = s + sl;
        put32(out, 0x0005);
        put32(out + 4, DPPLAYERTYPE_PLAYER);
        put32(out + 8, ev->id);
        put32(out + 0x14, ev->n ? buf + d : 0);
        put32(out + 0x18, ev->n);
        put32(out + 0x1c, 0x10);
        put32(out + 0x24, buf + s);
        put32(out + 0x28, buf + l);
        memcpy(out + d, ev->data, ev->n);
        memcpy(out + s, ev->sname, sl);
        memcpy(out + l, ev->lname, ll);
        return l + ll;
    }
    case NS_EV_HOST:   /* DPMSG_HOST: the type only */
        put32(out, 0x0101);
        return 4;
    case NS_EV_DATA: {   /* DPMSG_SETPLAYERORGROUPDATA, 0x14, then the data */
        put32(out, 0x0102);
        put32(out + 4, DPPLAYERTYPE_PLAYER);
        put32(out + 8, ev->id);
        put32(out + 0xc, buf + 0x14);
        put32(out + 0x10, ev->n);
        memcpy(out + 0x14, ev->data, ev->n);
        return 0x14 + ev->n;
    }
    }
    return 0;
}

/* Receive(lpidFrom, lpidTo, dwFlags, lpData, lpdwDataSize) */
static uint32_t m_receive(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t pfrom = dx_arg(a, n, 1), pto = dx_arg(a, n, 2), flags = dx_arg(a, n, 3);
    uint32_t data = dx_arg(a, n, 4), psize = dx_arg(a, n, 5);
    if (!gok(pfrom, 4) || !gok(pto, 4) || !gok(psize, 4)) return DPERR_INVALIDPARAMS;
    if (flags & ~(RC_ALL | RC_PEEK) & 0xffu) dx_log_once("IDirectPlay3A::Receive: flags %x taken as DPRECEIVE_ALL", flags);
    static NsEvent ev;   /* under the host lock */
    if (ns_recv(&ev, 0) != 0) return DPERR_NOMESSAGES;
    static uint8_t out[0x500];
    uint32_t size = build(&ev, data, out);
    if (size > NW_MAX_GAME) {   /* cannot happen (the sizes are capped): never block the queue on it */
        ns_recv(&ev, 1);
        return DPERR_NOMESSAGES;
    }
    if (!data || rt_r32(psize) < size || !gok(data, size)) {
        rt_w32(psize, size);
        return DPERR_BUFFERTOOSMALL;
    }
    if (!(flags & RC_PEEK)) ns_recv(&ev, 1);
    gwrite(data, out, size);
    if (ev.kind != NS_EV_GAME) net_log("Receive: system message %s about %08x", ev.kind == NS_EV_CREATE ? "CREATEPLAYERORGROUP" :
            ev.kind == NS_EV_DESTROY ? "DESTROYPLAYERORGROUP" : ev.kind == NS_EV_HOST ? "HOST" : "SETPLAYERORGROUPDATA", ev.id);
    else if (net_log_on() && ev.reliable) net_log("Receive: from %08x to %08x, %u bytes, type %02x kind %02x", ev.from, ev.to, ev.n, ev.data[0], ev.n > 1 ? ev.data[1] : 0);
    rt_w32(pfrom, ev.kind == NS_EV_GAME ? ev.from : 0);   /* DPID_SYSMSG */
    rt_w32(pto, ev.to);
    rt_w32(psize, size);
    return DP_OK;
}

/* Send(idFrom, idTo, dwFlags, lpData, dwDataSize) */
static uint32_t m_send(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t data = dx_arg(a, n, 4), size = dx_arg(a, n, 5);
    if (size > NW_MAX_GAME) return DPERR_SENDTOOBIG;
    if (!size || !gok(data, size)) return DPERR_INVALIDPARAMS;
    uint8_t buf[NW_MAX_GAME];
    gread(data, buf, size);
    if (net_log_on() && (dx_arg(a, n, 3) & 1))
        net_log("Send: %08x to %08x, %u bytes, type %02x kind %02x", dx_arg(a, n, 1), dx_arg(a, n, 2), size, buf[0], size > 1 ? buf[1] : 0);
    return ns_send(dx_arg(a, n, 1), dx_arg(a, n, 2), dx_arg(a, n, 3), buf, (int)size);
}

/* SetPlayerData(idPlayer, lpData, dwDataSize, dwFlags) */
static uint32_t m_set_player_data(const uint32_t* a, int n)
{
    if (!state_of(a, n)) return DPERR_INVALIDOBJECT;
    uint32_t data = dx_arg(a, n, 2), size = dx_arg(a, n, 3);
    if (size > NW_MAX_PDATA || (size && !gok(data, size))) return DPERR_INVALIDPARAMS;
    uint8_t buf[NW_MAX_PDATA];
    if (size) gread(data, buf, size);
    net_log("SetPlayerData %08x, %u bytes, flags %x", dx_arg(a, n, 1), size, dx_arg(a, n, 4));
    return ns_set_player_data(dx_arg(a, n, 1), buf, (int)size, dx_arg(a, n, 4));
}

/* ---- the slots nobody calls: a log line and DPERR_UNSUPPORTED ---- */

#define UNSUP(i, nm) \
    static uint32_t u_##i##_##nm(const uint32_t* a, int n) \
    { \
        (void)a; (void)n; \
        dx_log_once(#i "::" #nm ": not supported (DPERR_UNSUPPORTED)"); \
        return DPERR_UNSUPPORTED; \
    }
#define U(i, slot, nm) {slot, u_##i##_##nm, #i "::" #nm}

UNSUP(IDirectPlay, AddPlayerToGroup) UNSUP(IDirectPlay, CreateGroup) UNSUP(IDirectPlay, DeletePlayerFromGroup)
UNSUP(IDirectPlay, DestroyGroup) UNSUP(IDirectPlay, EnumGroupPlayers) UNSUP(IDirectPlay, EnumGroups)
UNSUP(IDirectPlay, GetPlayerCaps) UNSUP(IDirectPlay, SaveSession) UNSUP(IDirectPlay, SetPlayerName)
/* the v1 forms of the session calls: different argument lists, not used by the game */
UNSUP(IDirectPlay, CreatePlayer) UNSUP(IDirectPlay, DestroyPlayer) UNSUP(IDirectPlay, EnumPlayers)
UNSUP(IDirectPlay, EnumSessions) UNSUP(IDirectPlay, GetCaps) UNSUP(IDirectPlay, GetMessageCount)
UNSUP(IDirectPlay, GetPlayerName) UNSUP(IDirectPlay, Open) UNSUP(IDirectPlay, Receive) UNSUP(IDirectPlay, Send)

UNSUP(IDirectPlay3A, AddPlayerToGroup) UNSUP(IDirectPlay3A, CreateGroup) UNSUP(IDirectPlay3A, DeletePlayerFromGroup)
UNSUP(IDirectPlay3A, DestroyGroup) UNSUP(IDirectPlay3A, EnumGroupPlayers) UNSUP(IDirectPlay3A, EnumGroups)
UNSUP(IDirectPlay3A, GetGroupData) UNSUP(IDirectPlay3A, GetGroupName) UNSUP(IDirectPlay3A, GetPlayerAddress)
UNSUP(IDirectPlay3A, GetPlayerCaps) UNSUP(IDirectPlay3A, SetGroupData) UNSUP(IDirectPlay3A, SetGroupName)
UNSUP(IDirectPlay3A, SetPlayerName) UNSUP(IDirectPlay3A, SetSessionDesc) UNSUP(IDirectPlay3A, AddGroupToGroup)
UNSUP(IDirectPlay3A, CreateGroupInGroup) UNSUP(IDirectPlay3A, DeleteGroupFromGroup) UNSUP(IDirectPlay3A, EnumConnections)
UNSUP(IDirectPlay3A, EnumGroupsInGroup) UNSUP(IDirectPlay3A, GetGroupConnectionSettings)
UNSUP(IDirectPlay3A, InitializeConnection) UNSUP(IDirectPlay3A, SecureOpen) UNSUP(IDirectPlay3A, SendChatMessage)
UNSUP(IDirectPlay3A, SetGroupConnectionSettings) UNSUP(IDirectPlay3A, StartSession) UNSUP(IDirectPlay3A, GetGroupFlags)
UNSUP(IDirectPlay3A, GetGroupParent) UNSUP(IDirectPlay3A, GetPlayerAccount) UNSUP(IDirectPlay3A, GetPlayerFlags)

static const DxMethod M_DP1[] = {
    {0x00, m_qi, "IDirectPlay::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectPlay::AddRef"},
    {0x08, dx_unk_release, "IDirectPlay::Release"},
    U(IDirectPlay, 0x0c, AddPlayerToGroup),
    {0x10, m_close, "IDirectPlay::Close"},
    U(IDirectPlay, 0x14, CreatePlayer),
    U(IDirectPlay, 0x18, CreateGroup),
    U(IDirectPlay, 0x1c, DeletePlayerFromGroup),
    U(IDirectPlay, 0x20, DestroyPlayer),
    U(IDirectPlay, 0x24, DestroyGroup),
    {0x28, m_enable_new_players, "IDirectPlay::EnableNewPlayers"},
    U(IDirectPlay, 0x2c, EnumGroupPlayers),
    U(IDirectPlay, 0x30, EnumGroups),
    U(IDirectPlay, 0x34, EnumPlayers),
    U(IDirectPlay, 0x38, EnumSessions),
    U(IDirectPlay, 0x3c, GetCaps),
    U(IDirectPlay, 0x40, GetMessageCount),
    U(IDirectPlay, 0x44, GetPlayerCaps),
    U(IDirectPlay, 0x48, GetPlayerName),
    {0x4c, m_initialize, "IDirectPlay::Initialize"},
    U(IDirectPlay, 0x50, Open),
    U(IDirectPlay, 0x54, Receive),
    U(IDirectPlay, 0x58, SaveSession),
    U(IDirectPlay, 0x5c, Send),
    U(IDirectPlay, 0x60, SetPlayerName),
};

static const DxMethod M_DP3[] = {
    {0x00, m_qi, "IDirectPlay3A::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectPlay3A::AddRef"},
    {0x08, dx_unk_release, "IDirectPlay3A::Release"},
    U(IDirectPlay3A, 0x0c, AddPlayerToGroup),
    {0x10, m_close, "IDirectPlay3A::Close"},
    U(IDirectPlay3A, 0x14, CreateGroup),
    {0x18, m_create_player, "IDirectPlay3A::CreatePlayer"},
    U(IDirectPlay3A, 0x1c, DeletePlayerFromGroup),
    U(IDirectPlay3A, 0x20, DestroyGroup),
    {0x24, m_destroy_player, "IDirectPlay3A::DestroyPlayer"},
    U(IDirectPlay3A, 0x28, EnumGroupPlayers),
    U(IDirectPlay3A, 0x2c, EnumGroups),
    {0x30, m_enum_players, "IDirectPlay3A::EnumPlayers"},
    {0x34, m_enum_sessions, "IDirectPlay3A::EnumSessions"},
    {0x38, m_get_caps, "IDirectPlay3A::GetCaps"},
    U(IDirectPlay3A, 0x3c, GetGroupData),
    U(IDirectPlay3A, 0x40, GetGroupName),
    {0x44, m_get_message_count, "IDirectPlay3A::GetMessageCount"},
    U(IDirectPlay3A, 0x48, GetPlayerAddress),
    U(IDirectPlay3A, 0x4c, GetPlayerCaps),
    {0x50, m_get_player_data, "IDirectPlay3A::GetPlayerData"},
    {0x54, m_get_player_name, "IDirectPlay3A::GetPlayerName"},
    {0x58, m_get_session_desc, "IDirectPlay3A::GetSessionDesc"},
    {0x5c, m_initialize, "IDirectPlay3A::Initialize"},
    {0x60, m_open, "IDirectPlay3A::Open"},
    {0x64, m_receive, "IDirectPlay3A::Receive"},
    {0x68, m_send, "IDirectPlay3A::Send"},
    U(IDirectPlay3A, 0x6c, SetGroupData),
    U(IDirectPlay3A, 0x70, SetGroupName),
    {0x74, m_set_player_data, "IDirectPlay3A::SetPlayerData"},
    U(IDirectPlay3A, 0x78, SetPlayerName),
    U(IDirectPlay3A, 0x7c, SetSessionDesc),
    U(IDirectPlay3A, 0x80, AddGroupToGroup),
    U(IDirectPlay3A, 0x84, CreateGroupInGroup),
    U(IDirectPlay3A, 0x88, DeleteGroupFromGroup),
    U(IDirectPlay3A, 0x8c, EnumConnections),
    U(IDirectPlay3A, 0x90, EnumGroupsInGroup),
    U(IDirectPlay3A, 0x94, GetGroupConnectionSettings),
    U(IDirectPlay3A, 0x98, InitializeConnection),
    U(IDirectPlay3A, 0x9c, SecureOpen),
    U(IDirectPlay3A, 0xa0, SendChatMessage),
    U(IDirectPlay3A, 0xa4, SetGroupConnectionSettings),
    U(IDirectPlay3A, 0xa8, StartSession),
    U(IDirectPlay3A, 0xac, GetGroupFlags),
    U(IDirectPlay3A, 0xb0, GetGroupParent),
    U(IDirectPlay3A, 0xb4, GetPlayerAccount),
    U(IDirectPlay3A, 0xb8, GetPlayerFlags),
};

#define NM(t) ((int)(sizeof(t) / sizeof((t)[0])))

/* ---- the DPLAYX entry points ---- */

static uint32_t sp_guid_addr, sp_name_addr;

static uint32_t imp_dplay_enumerate(const uint32_t* a, int n)
{
    uint32_t cb = dx_arg(a, n, 0), ctx = dx_arg(a, n, 1);
    if (!cb) return DPERR_INVALIDPARAMS;
    if (!ns_available()) {
        dx_log_once("DirectPlayEnumerateA: no sockets on this platform: no service providers");
        return DP_OK;
    }
    if (!sp_guid_addr) {
        sp_guid_addr = rt_fake_alloc(16);
        gwrite(sp_guid_addr, SP_GUID, 16);
        sp_name_addr = rt_fake_alloc((uint32_t)sizeof SP_NAME);
        gwrite(sp_name_addr, SP_NAME, (uint32_t)sizeof SP_NAME);
    }
    net_log("DirectPlayEnumerateA: \"%s\"", SP_NAME);
    /* LPDPENUMDPCALLBACKA(lpguidSP, lpSPName, dwMajorVersion, dwMinorVersion, lpContext) */
    uint32_t w[5] = {sp_guid_addr, sp_name_addr, 5, 0, ctx};
    dx_call_cb(cb, w, 5);
    return DP_OK;
}

static uint32_t imp_dplay_create(const uint32_t* a, int n)
{
    uint32_t guid = dx_arg(a, n, 0), out = dx_arg(a, n, 1), outer = dx_arg(a, n, 2);
    if (!gok(out, 4)) return DPERR_INVALIDPARAMS;
    rt_w32(out, 0);
    if (outer) return CLASS_E_NOAGGREGATION;
    uint8_t g[16];
    if (!gok(guid, 16) || gread(guid, g, 16) != 0 || memcmp(g, SP_GUID, 16) != 0 || !ns_available()) {
        dx_log_once("DirectPlayCreate: not our service provider");
        return DPERR_UNAVAILABLE;
    }
    DpState* s = calloc(1, sizeof *s);
    if (!s) return DPERR_GENERIC;
    s->v1 = dx_obj_new(&I_DP1, s, v1_destroy);
    if (!s->v1) { free(s); return DPERR_GENERIC; }
    s->refs = 1;
    rt_w32(out, s->v1->addr);
    return DP_OK;
}

static void rt_bind_host_net_dplay(void)
{
    dx_iface_build(&I_DP1, M_DP1, NM(M_DP1), 0x64 / 4, 0);
    dx_iface_build(&I_DP3, M_DP3, NM(M_DP3), 0xbc / 4, 0);
    /* imported by ordinal: the runtime names them "DPLAYX.dll!#n" from the EXE's import table */
    rt_bind_import("DPLAYX.dll!#2", imp_dplay_enumerate);   /* DirectPlayEnumerateA */
    rt_bind_import("DPLAYX.dll!#1", imp_dplay_create);      /* DirectPlayCreate */
}

RT_BIND_AREA(host_net_dplay)
