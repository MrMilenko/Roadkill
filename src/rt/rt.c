/*
 * rt.c: the runtime's address space, loader, frames, heap and call table (see rt.h).
 */
#include "rt.h"
#include "math/tm2math.h"

#include <stdio.h>
#include <stdlib.h>
#include "host/os/os.h"

uint8_t* rt_base;

static uint32_t heap_top = RT_HEAP_BASE;
/* frames: every thread has its own slice of the frame area, growing down. The main thread gets
 * the top 8 MB; each game thread (rt_thread_begin) a further 512 KB slice. */
#define RT_MAIN_FRAMES   0x00800000u
#define RT_THREAD_FRAMES 0x00080000u
#ifdef ROADKILL_NO_TLS
typedef struct { uint32_t top, lo, committed; } RtFrames;
#define RT_FR ((RtFrames*)os_tls(0))
#define stack_top (RT_FR->top)
#define stack_lo (RT_FR->lo)
#define frames_committed (RT_FR->committed)
#else
static __thread uint32_t stack_top, stack_lo, frames_committed;
#endif
/* commits this thread's frames down to stack_top (os_commit: only the Xbox needs it) */
static void frames_commit(void)
{
    if (stack_top >= frames_committed) return;
    uint32_t lo = stack_top & ~0xffffu;
    if (lo < stack_lo) lo = stack_lo;
    if (os_commit(rt_ptr(lo), frames_committed - lo) != 0) rt_stop("frames: out of memory", frames_committed - lo);
    frames_committed = lo;
}
static uint32_t next_slice = RT_FAKE_BASE - RT_MAIN_FRAMES;   /* guarded by slice_lock */
#include <pthread.h>
static pthread_mutex_t slice_lock = PTHREAD_MUTEX_INITIALIZER;

static void frames_init_main(void)
{
    stack_top = RT_FAKE_BASE;
    stack_lo = RT_FAKE_BASE - RT_MAIN_FRAMES;
    frames_committed = RT_FAKE_BASE;
}

void rt_thread_begin(void)
{
    pthread_mutex_lock(&slice_lock);
    uint32_t top = next_slice;
    if (top - RT_THREAD_FRAMES < RT_STACK_END) { pthread_mutex_unlock(&slice_lock); rt_stop("no frame slice left for a thread", 0); }
    next_slice -= RT_THREAD_FRAMES;
    pthread_mutex_unlock(&slice_lock);
    stack_top = top;
    stack_lo = top - RT_THREAD_FRAMES;
    frames_committed = top;
}
static uint32_t fake_top = RT_FAKE_BASE;
static struct { uint32_t slot; char name[64]; } imports[256];
static int nimports;

/* ---- the image ---- */

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

int rt_init(const char* exe_path)
{
    rt_base = os_reserve(RT_SIZE);
    if (!rt_base) return -1;
    /* the low 64 KB (the original's null pointer reads land there) and the fake area */
    os_commit(rt_base, 0x10000);
    os_commit(rt_base + RT_FAKE_BASE, RT_SIZE - RT_FAKE_BASE);
    FILE* f = fopen(exe_path, "rb");
    if (!f) { fprintf(stderr, "tm2: cannot open %s\n", exe_path); return -1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* d = malloc((size_t)size);
    if (!d || fread(d, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(d); return -1; }
    fclose(f);
    if (size < 0x40 || d[0] != 'M' || d[1] != 'Z') { free(d); return -1; }
    uint32_t pe = rd32(d + 0x3c);
    if (pe + 0xf8 > (uint32_t)size || memcmp(d + pe, "PE\0\0", 4)) { free(d); return -1; }
    uint32_t nsec = rd16(d + pe + 6), opt = rd16(d + pe + 20), base = rd32(d + pe + 52);
    if (base != 0x00400000u) { fprintf(stderr, "tm2: %s is not the TM2.EXE this port reads\n", exe_path); free(d); return -1; }
    os_commit(rt_base + base, 0x1000);
    uint32_t committed_to = base + 0x1000;
    memcpy(rt_base + base, d, 0x1000);   /* the headers, as the loader maps them */
    for (uint32_t i = 0; i < nsec; i++) {
        const uint8_t* s = d + pe + 24 + opt + 40 * i;
        /* Watcom's linker leaves VirtualSize 0: the raw data is the whole section */
        uint32_t vs = rd32(s + 8), va = rd32(s + 12), rs = rd32(s + 16), rp = rd32(s + 20);
        uint32_t n = vs > rs ? vs : rs;
        if (base + va + n > RT_HEAP_BASE) { free(d); return -1; }
        /* the image is committed whole, from the headers to the last section's end: the gaps
         * between sections too (code reads the image as one block, e.g. ai_cars_respawn's start
         * set table) */
        if (base + va + n > committed_to) {
            if (os_commit(rt_base + committed_to, base + va + n - committed_to) != 0) { free(d); return -1; }
            committed_to = base + va + n;
        }
        if (rp == 0) continue;   /* .bss: no file data, the mapping is already zero */
        if (rp + rs > (uint32_t)size) { free(d); return -1; }
        memcpy(rt_base + base + va, d + rp, rs);
    }
    /* the import table: IAT slot -> "DLL!name", so host services bind imports by name */
    uint32_t imp = rd32(d + pe + 24 + 104);
    if (imp) {
        for (uint32_t o = base + imp;; o += 20) {
            uint32_t oft = rt_r32(o), dllname = rt_r32(o + 12), ft = rt_r32(o + 16);
            if (!dllname) break;
            for (uint32_t i = 0;; i++) {
                uint32_t ent = rt_r32(base + (oft ? oft : ft) + 4 * i);
                if (!ent) break;
                if (nimports < (int)(sizeof imports / sizeof imports[0])) {
                    imports[nimports].slot = base + ft + 4 * i;
                    if (ent & 0x80000000u)   /* by ordinal: "DLL!#n" */
                        snprintf(imports[nimports].name, sizeof imports[nimports].name, "%s!#%u",
                                 (const char*)rt_ptr(base + dllname), ent & 0xffffu);
                    else
                        snprintf(imports[nimports].name, sizeof imports[nimports].name, "%s!%s",
                                 (const char*)rt_ptr(base + dllname), (const char*)rt_ptr(base + ent + 2));
                    nimports++;
                }
            }
        }
    }
    free(d);
    /* the fixed point math tables the ports read on the host side: SINTAB.DAT beside the EXE */
    char sintab[1024];
    const char* slash = strrchr(exe_path, '/');
    snprintf(sintab, sizeof sintab, "%.*sSINTAB.DAT", slash ? (int)(slash - exe_path + 1) : 0, exe_path);
    if (tm2_math_load_tables(sintab, exe_path) != 0) { fprintf(stderr, "tm2: cannot load %s\n", sintab); return -1; }
    return 0;
}

uint32_t rt_import_slot(const char* name)
{
    for (int i = 0; i < nimports; i++) {
        const char* bang = strchr(imports[i].name, '!');
        if (!strcmp(imports[i].name, name) || (bang && !strcmp(bang + 1, name))) return imports[i].slot;
    }
    return 0;
}

const char* rt_import_name(uint32_t slot)
{
    for (int i = 0; i < nimports; i++) if (imports[i].slot == slot) return imports[i].name;
    return NULL;
}

void rt_bind_import(const char* name, RtFn fn)
{
    uint32_t slot = rt_import_slot(name);
    if (slot) rt_bind(slot, fn, name);
}

volatile uint32_t rt_world_updates;

int rt_is_host_ptr(uintptr_t v)
{
#if UINTPTR_MAX > 0xffffffffu
    return v > 0xffffffffu;
#else
    if (v >= (uintptr_t)rt_base && v < (uintptr_t)rt_base + RT_SIZE) return 1;
    return os_on_stack((const void*)v);
#endif
}

void rt_shutdown(void)
{
    if (rt_base) os_release(rt_base, RT_SIZE);
    rt_base = NULL;
}

/* ---- frames, heap, fakes ---- */

uint32_t rt_frame_push(uint32_t n)
{
    n = (n + 15u) & ~15u;
    if (!stack_top) frames_init_main();
    if (stack_top - n < stack_lo) rt_stop("frame stack full", n);
    stack_top -= n;
    frames_commit();
    memset(rt_ptr(stack_top), 0, n);
    return stack_top;
}

void rt_frame_pop(uint32_t n) { stack_top += (n + 15u) & ~15u; }

uint32_t rt_heap_grow(uint32_t n)
{
    n = (n + 15u) & ~15u;
    if (heap_top + n > RT_HEAP_END) return 0;
    uint32_t a = heap_top;
    if (os_commit(rt_ptr(a), n) != 0) return 0;
    heap_top += n;
    memset(rt_ptr(a), 0, n);
    return a;
}

uint32_t rt_fake_alloc(uint32_t n)
{
    n = (n + 15u) & ~15u;
    if (fake_top + n > RT_SIZE) rt_stop("fake area full", n);
    uint32_t a = fake_top;
    fake_top += n;
    return a;
}

/* ---- calls ---- */

typedef struct { uint32_t addr; RtFn fn; const char* name; int weak; int arity; int warned; } Bind;
static struct { uint32_t addr; int k; } arities[2048];
static int narities;
static Bind* binds;
static int nbinds, capbinds, sorted;

void rt_bind(uint32_t addr, RtFn fn, const char* name)
{
    if (nbinds == capbinds) {
        capbinds = capbinds ? capbinds * 2 : 1024;
        binds = realloc(binds, sizeof(Bind) * (size_t)capbinds);
    }
    binds[nbinds++] = (Bind){addr, fn, name, 0, -1, 0};
    sorted = 0;
}

void rt_bind_weak(uint32_t addr, RtFn fn, const char* name)
{
    rt_bind(addr, fn, name);
    binds[nbinds - 1].weak = 1;
}

static int cmp_bind(const void* a, const void* b)
{
    uint32_t x = ((const Bind*)a)->addr, y = ((const Bind*)b)->addr;
    return x < y ? -1 : x > y;
}

static const Bind* find(uint32_t addr)
{
    if (!sorted) {
        qsort(binds, (size_t)nbinds, sizeof(Bind), cmp_bind);
        int w = 0;
        for (int i = 0; i < nbinds; i++) {
            if (w && binds[w - 1].addr == binds[i].addr) {
                if (binds[i].weak) continue;                       /* a weak binding yields */
                if (binds[w - 1].weak) { binds[w - 1] = binds[i]; continue; }
                fprintf(stderr, "tm2: 0x%08x bound twice (%s, %s)\n", binds[i].addr, binds[w - 1].name, binds[i].name);
                exit(3);
            }
            binds[w++] = binds[i];
        }
        nbinds = w;
        sorted = 1;
        for (int i = 0; i < narities; i++) {
            Bind* b = (Bind*)find(arities[i].addr);
            if (b) b->arity = arities[i].k;
        }
    }
    int lo = 0, hi = nbinds - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (binds[m].addr == addr) return &binds[m];
        if (binds[m].addr < addr) lo = m + 1; else hi = m - 1;
    }
    return NULL;
}

int rt_bind_count(void) { return nbinds; }

void rt_set_arity(uint32_t addr, int k)
{
    if (narities < 2048) { arities[narities].addr = addr; arities[narities].k = k; narities++; }
}

void rt_list_binds(FILE* f)
{
    find(0);   /* sorts */
    for (int i = 0; i < nbinds; i++) fprintf(f, "%08x\t%s\n", binds[i].addr, binds[i].name);
}

const char* rt_name(uint32_t addr) { const Bind* b = find(addr); return b ? b->name : NULL; }

#define CHAIN 64
#ifdef ROADKILL_NO_TLS
typedef struct { uint32_t chain[CHAIN]; int depth; } RtChain;
#define RT_CH ((RtChain*)os_tls(1))
#define chain (RT_CH->chain)
#define depth (RT_CH->depth)
#else
static __thread uint32_t chain[CHAIN];
static __thread int depth;
#endif
static int trace = -1;

void rt_stop(const char* why, uint32_t addr)
{
    const char* in = rt_import_name(addr);
    fprintf(stderr, "tm2: stop: %s 0x%08x%s%s\n", why, addr, in ? " " : "", in ? in : "");
    for (int i = depth - 1; i >= 0 && i >= depth - CHAIN; i--) {
        const char* nm = rt_name(chain[i % CHAIN]);
        fprintf(stderr, "  called from 0x%08x %s\n", chain[i % CHAIN], nm ? nm : "");
    }
    exit(4);
}

uint32_t rt_call(uint32_t fn, const uint32_t* args, int n)
{
    if (trace < 0) { const char* t = getenv("TM2_TRACE"); trace = t && *t == '1'; }
    const Bind* b = find(fn);
    if (trace) {
        fprintf(stderr, "%*s0x%08x %s(", depth > 40 ? 40 : depth, "", fn, b ? b->name : "?");
        for (int i = 0; i < n; i++) fprintf(stderr, i ? ", %x" : "%x", args[i]);
        fprintf(stderr, ")\n");
    }
    if (!b && fn >= 0x00410000u && fn < 0x004e0000u && rt_r8(fn) == 0xffu && rt_r8(fn + 1) == 0x25u) {
        /* an import thunk of the EXE, jmp [IAT slot]: forward to the slot's binding */
        uint32_t slot = rt_r32(fn + 2);
        if (rt_import_name(slot)) return rt_call(slot, args, n);
    }
    if (!b) rt_stop("no binding for", fn);
    if (b->arity > n && !b->warned && getenv("TM2_ARITY")) {   /* developer check: TM2_ARITY=1 warns, =stop stops */
        ((Bind*)b)->warned = 1;
        const char* pn = depth > 0 ? rt_name(chain[(depth - 1) % CHAIN]) : NULL;
        fprintf(stderr, "tm2: arity: 0x%08x %s takes %d arguments, called with %d from 0x%08x %s\n", fn, b->name,
                b->arity, n, depth > 0 ? chain[(depth - 1) % CHAIN] : 0, pn ? pn : "");
        const char* st = getenv("TM2_ARITY");
        if (st && !strcmp(st, "stop")) rt_stop("arity", fn);
    }
    /* the ports that read the frame's 60 Hz ticks on the host side see the game's value
     * (frame_timer_update writes 0x004e8300) */
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
    chain[depth % CHAIN] = fn;
    depth++;
    uint32_t r = b->fn(args, n);
    depth--;
    return r;
}

/* ---- callbacks for the env structs ---- */

uint8_t* rt_mem_cb(void* user, uint32_t addr) { (void)user; return rt_ptr(addr); }
uint32_t rt_call_cb(void* user, uint32_t fn, const uint32_t* args, int n) { (void)user; return rt_call(fn, args, n); }
uint32_t rt_call_n_cb(void* user, uint32_t fn, int n, const uint32_t* args) { (void)user; return rt_call(fn, args, n); }
