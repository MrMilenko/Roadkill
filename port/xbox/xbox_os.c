/*
 * xbox_os.c: host/os/os.h on the original Xbox (port code).
 *
 * Memory: the game's 64 MB address space is reserved, and committed only where rt.c says the
 * game touches it, as the console has no paging file. Per-thread blocks (os_tls): the compiler
 * setup here has no __thread, so each kernel thread (KPRCB.CurrentThread, fs:[0x28]) gets a slot
 * of blocks; a thread's slot is given back when it ends (xbox_pthread.c).
 */
#include <xtl.h>
#include <string.h>
#include "host/os/os.h"

void* os_reserve(size_t n) { return VirtualAlloc(NULL, n, MEM_RESERVE, PAGE_READWRITE); }
void os_release(void* p, size_t n) { (void)n; VirtualFree(p, 0, MEM_RELEASE); }

int os_commit(void* p, size_t n)
{
    if (!n) return 0;
    void* r = VirtualAlloc(p, n, MEM_COMMIT, PAGE_READWRITE);
    if (r) return 0;
    {
        MEMORYSTATUS ms;
        memset(&ms, 0, sizeof ms);
        ms.dwLength = sizeof ms;
        GlobalMemoryStatus(&ms);
        rk_log("os_commit: %u bytes at %p refused (%u KB free)", (unsigned)n, p, (unsigned)(ms.dwAvailPhys >> 10));
    }
    return -1;
}

char* os_stack_top(void)
{
    char* top;
    __asm__ volatile("movl %%fs:4, %0" : "=r"(top));   /* NT_TIB.StackBase of this thread */
    return top;
}

int os_mkdir(const char* path)
{
    char p[MAX_PATH];
    size_t i;
    for (i = 0; path[i] && i + 1 < sizeof p; i++) p[i] = path[i] == '/' ? '\\' : path[i];
    p[i] = 0;
    return CreateDirectory(p, NULL) || GetLastError() == ERROR_ALREADY_EXISTS ? 0 : -1;
}

int os_setenv(const char* name, const char* value) { return rk_setenv(name, value, 1); }

/* ---- per-thread blocks ---- */

#define TLS_SLOTS 32
#define TLS_KEYS 8
#define TLS_BLOCK 512
typedef struct { void* thread; unsigned char block[TLS_KEYS][TLS_BLOCK]; } TlsSlot;
static TlsSlot slots[TLS_SLOTS];
static volatile LONG slots_lock;

static void* current_thread(void)
{
    void* t;
    __asm__ volatile("movl %%fs:0x28, %0" : "=r"(t));   /* KPCR.PrcbData.CurrentThread */
    return t;
}

void* os_tls(int key)
{
    void* t = current_thread();
    int i;
    for (i = 0; i < TLS_SLOTS; i++) if (slots[i].thread == t) return slots[i].block[key];
    while (InterlockedCompareExchange(&slots_lock, 1, 0) != 0) Sleep(0);
    for (i = 0; i < TLS_SLOTS; i++) if (!slots[i].thread) break;
    if (i == TLS_SLOTS) { slots_lock = 0; rk_fatal("more than 32 threads"); }
    memset(slots[i].block, 0, sizeof slots[i].block);
    slots[i].thread = t;
    slots_lock = 0;
    return slots[i].block[key];
}

void rk_tls_release(void)
{
    void* t = current_thread();
    int i;
    for (i = 0; i < TLS_SLOTS; i++) if (slots[i].thread == t) slots[i].thread = NULL;
}
