/*
 * dx_com.c: fake COM objects in game memory (see dx_com.h). Port code, not from the original.
 */
#include "dx_com.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/pf2_env.h"
#include "host/settings/settings.h"

#define DX_MAGIC 0x424f5844u   /* "DXOB" */
#define DX_MAXOBJ 8192

static DxObj* objs[DX_MAXOBJ];
static uint32_t free_addr[DX_MAXOBJ];   /* game addresses of released objects, reused */
static int nfree;
static int nobjs;                        /* high water mark of objs[] */

int (*dx_qi_hook)(DxObj* o, uint32_t iid_data1, uint32_t out, uint32_t* hr);

void dx_log_once(const char* fmt, ...)
{
    static char* seen[512];
    static int nseen, all = -1;
    if (all < 0) all = settings_bool("debug.dx_log", 0);
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (!all) {
        for (int i = 0; i < nseen; i++) if (!strcmp(seen[i], buf)) return;
        if (nseen < 512) {
            size_t len = strlen(buf) + 1;
            seen[nseen] = malloc(len);
            if (seen[nseen]) memcpy(seen[nseen++], buf, len);
        }
    }
    fprintf(stderr, "tm2 host: %s\n", buf);
}

/* a vtable slot nothing implements: the stop names the slot (rt_stop prints the bound name of
 * the called address on top of the chain) */
static uint32_t not_implemented(const uint32_t* a, int n)
{
    (void)a; (void)n;
    rt_stop("COM method not implemented by the host", 0);
    return DX_E_NOTIMPL;
}

void dx_iface_build(DxIface* i, const DxMethod* m, int nm, int nslots, int pf2_iface)
{
    uint32_t vt = rt_fake_alloc(4u * (uint32_t)nslots);
    uint32_t code = rt_fake_alloc(4u * (uint32_t)nslots);
    for (int s = 0; s < nslots; s++) {
        uint32_t slot = 4u * (uint32_t)s, entry = code + slot;
        rt_w32(vt + slot, entry);
        const DxMethod* f = NULL;
        for (int k = 0; k < nm; k++) if (m[k].slot == slot) f = &m[k];
        if (f) {
            rt_bind(entry, f->fn, f->name);
            if (pf2_iface) rt_bind(PF2_COM(pf2_iface, slot), f->fn, f->name);
        } else {
            char nm2[96];
            snprintf(nm2, sizeof nm2, "%s slot 0x%02x (not implemented)", i->name, slot);
            size_t len = strlen(nm2) + 1;
            char* p = malloc(len);
            if (p) memcpy(p, nm2, len);
            rt_bind(entry, not_implemented, p ? p : "COM slot (not implemented)");
            if (pf2_iface) rt_bind(PF2_COM(pf2_iface, slot), not_implemented, p ? p : "COM slot (not implemented)");
        }
    }
    i->vtable = vt;
}

DxObj* dx_obj_new(DxIface* i, void* host, DxDestroy destroy)
{
    int idx = -1;
    for (int k = 0; k < nobjs; k++) if (!objs[k]) { idx = k; break; }
    if (idx < 0) {
        if (nobjs >= DX_MAXOBJ) { rt_stop("host COM object table full", 0); return NULL; }
        idx = nobjs++;
    }
    DxObj* o = calloc(1, sizeof *o);
    if (!o) return NULL;
    o->addr = nfree ? free_addr[--nfree] : rt_fake_alloc(16);
    o->iface = i;
    o->refs = 1;
    o->host = host;
    o->destroy = destroy;
    rt_w32(o->addr, i->vtable);
    rt_w32(o->addr + 4, DX_MAGIC);
    rt_w32(o->addr + 8, (uint32_t)idx);
    objs[idx] = o;
    return o;
}

DxObj* dx_obj_get(uint32_t a, const DxIface* i)
{
    if (a < RT_FAKE_BASE || a > RT_SIZE - 16) return NULL;
    if (rt_r32(a + 4) != DX_MAGIC) return NULL;
    uint32_t idx = rt_r32(a + 8);
    if (idx >= (uint32_t)nobjs || !objs[idx] || objs[idx]->addr != a) return NULL;
    if (i && objs[idx]->iface != i) return NULL;
    return objs[idx];
}

uint32_t dx_obj_addref(DxObj* o) { return (uint32_t)++o->refs; }

uint32_t dx_obj_release(DxObj* o)
{
    if (o->refs <= 0) return 0;
    if (--o->refs > 0) return (uint32_t)o->refs;
    if (o->destroy) o->destroy(o);
    uint32_t idx = rt_r32(o->addr + 8);
    if (idx < (uint32_t)nobjs && objs[idx] == o) objs[idx] = NULL;
    /* the game may still hold the pointer (a dangling interface): the magic is cleared so a call
     * through it is refused, the vtable left so the call still reaches the host */
    rt_w32(o->addr + 4, 0);
    if (nfree < DX_MAXOBJ) free_addr[nfree++] = o->addr;
    free(o);
    return 0;
}

uint32_t dx_unk_qi(const uint32_t* a, int n)
{
    DxObj* o = dx_obj_get(dx_arg(a, n, 0), NULL);
    uint32_t iid = dx_arg(a, n, 1), out = dx_arg(a, n, 2);
    if (!o) return DX_E_POINTER;
    if (!out) return DX_E_POINTER;
    uint32_t d1 = iid ? rt_r32(iid) : 0;
    uint32_t hr;
    if (dx_qi_hook && dx_qi_hook(o, d1, out, &hr)) return hr;
    for (int k = 0; k < 6 && o->iface->iids[k]; k++)
        if (o->iface->iids[k] == d1 || d1 == 0x00000000u) {
            /* IID_IUnknown is {00000000-0000-0000-C000-000000000046} */
            dx_obj_addref(o);
            rt_w32(out, o->addr);
            return DX_OK;
        }
    dx_log_once("%s::QueryInterface: no interface %08x", o->iface->name, d1);
    rt_w32(out, 0);
    return DX_E_NOINTERFACE;
}

uint32_t dx_unk_addref(const uint32_t* a, int n)
{
    DxObj* o = dx_obj_get(dx_arg(a, n, 0), NULL);
    return o ? dx_obj_addref(o) : 0;
}

uint32_t dx_unk_release(const uint32_t* a, int n)
{
    DxObj* o = dx_obj_get(dx_arg(a, n, 0), NULL);
    return o ? dx_obj_release(o) : 0;
}

/* a game callback (stdcall, no register arguments): its words from a[0] (compact, docs/RUNTIME.md) */
uint32_t dx_call_cb(uint32_t fn, const uint32_t* words, int nwords)
{
    return rt_call(fn, words, nwords);
}

static void rt_bind_host_dx_com(void)
{
    /* IUnknown on any host object, for ports that call PF2_COM(PF2_IUNK, slot) */
    rt_bind(PF2_COM(PF2_IUNK, 0), dx_unk_qi, "IUnknown::QueryInterface");
    rt_bind(PF2_COM(PF2_IUNK, 4), dx_unk_addref, "IUnknown::AddRef");
    rt_bind(PF2_COM(PF2_IUNK, 8), dx_unk_release, "IUnknown::Release");
}

RT_BIND_AREA(host_dx_com)
