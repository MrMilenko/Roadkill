/*
 * dx_com.h: fake COM objects for the host services (DirectDraw, DirectSound, DirectInput). Port
 * code, not from the original: the game holds interface pointers in its own memory, so every
 * object the host hands out is a game address in the runtime's fake area (rt_fake_alloc):
 *
 *   +0x00 vtable (game address, also in the fake area)
 *   +0x04 'DXOB' magic
 *   +0x08 index into the host's object table
 *
 * A vtable entry k is a fake address bound (rt_bind) to the C function of that method, so a port
 * that reads the vtable from game memory and calls what it finds reaches the same function as one
 * that calls PF2_COM(interface, slot) (src/platform/pf2_env.h). Method adapters get the runtime's
 * COM arguments: a[0] = this, a[1..] = the stdcall words.
 *
 * Reference counts are per object (per interface pointer), as DirectDraw keeps them: the game's
 * ddraw_init 0x004b3d80 relies on IDirectDraw::Release returning 0 after QueryInterface made an
 * IDirectDraw2.
 */
#ifndef TM2_HOST_DX_COM_H
#define TM2_HOST_DX_COM_H

#include <stdint.h>

#include "rt.h"

#define DX_OK            0u
#define DX_E_NOTIMPL     0x80004001u   /* E_NOTIMPL = DDERR_UNSUPPORTED */
#define DX_E_NOINTERFACE 0x80004002u
#define DX_E_POINTER     0x80004003u
#define DX_E_FAIL        0x80004005u
#define DX_E_INVALIDARG  0x80070057u   /* DDERR_INVALIDPARAMS, DSERR_INVALIDPARAM, DIERR_INVALIDPARAM */

/* the i-th argument word, 0 when the caller passed fewer */
static inline uint32_t dx_arg(const uint32_t* a, int n, int i) { return i < n ? a[i] : 0; }

typedef struct DxObj DxObj;
typedef void (*DxDestroy)(DxObj* o);

/* one interface: its vtable in game memory and the IIDs (first dword, Data1) QueryInterface
 * answers with the object itself */
typedef struct DxIface {
    const char* name;
    uint32_t vtable;              /* game address, 0 until dx_iface_build */
    uint32_t iids[6];             /* Data1 of the IIDs this object answers to (0 ends) */
} DxIface;

struct DxObj {
    uint32_t addr;                /* the game address of the object (the interface pointer) */
    DxIface* iface;
    int refs;
    void* host;                   /* the kind's own state */
    DxDestroy destroy;            /* called when refs reaches 0 (may be NULL) */
};

typedef struct DxMethod {
    uint32_t slot;                /* vtable byte offset */
    RtFn fn;
    const char* name;
} DxMethod;

/* Builds the vtable of an interface: every slot below nslots * 4 gets a fake address; slots with
 * a method are bound to it (and, when pf2_iface != 0, PF2_COM(pf2_iface, slot) too); the others
 * to a stub that stops the run naming the slot. */
void dx_iface_build(DxIface* i, const DxMethod* m, int nm, int nslots, int pf2_iface);

/* a new object (refs 1) of interface i */
DxObj* dx_obj_new(DxIface* i, void* host, DxDestroy destroy);
/* the object at game address a (any interface when i is NULL), or NULL */
DxObj* dx_obj_get(uint32_t a, const DxIface* i);
uint32_t dx_obj_addref(DxObj* o);
uint32_t dx_obj_release(DxObj* o);   /* returns the new count; destroys at 0 */

/* the IUnknown methods every interface binds at slots 0, 4, 8 (QueryInterface answers with the
 * object itself for its iids; DirectDraw's IDirectDraw -> IDirectDraw2 is special cased by the
 * DirectDraw host through dx_qi_hook) */
uint32_t dx_unk_qi(const uint32_t* a, int n);
uint32_t dx_unk_addref(const uint32_t* a, int n);
uint32_t dx_unk_release(const uint32_t* a, int n);
/* a QueryInterface that needs a new object: returns 1 when handled (*hr set) */
extern int (*dx_qi_hook)(DxObj* o, uint32_t iid_data1, uint32_t out, uint32_t* hr);

/* PF2_COM(PF2_IUNK, slot) for slots past IUnknown's (0x10 / 0x30 / 0x74: the pf8 ports' DirectPlay
 * calls) are bound to a dispatch that looks the object up and calls its own vtable's entry: the
 * same slot is a different method on different interfaces (0x30: IDirectPlay::EnumGroups,
 * IDirectPlay3A::EnumPlayers). */

/* calls a game callback (Watcom function with stdcall stack words, as the generated bindings
 * read them: a[4 + i]) with up to 6 stack words */
uint32_t dx_call_cb(uint32_t fn, const uint32_t* words, int nwords);

/* one line to stderr the first time a given message is seen (TM2_DX_LOG=1: every time) */
void dx_log_once(const char* fmt, ...);

#endif /* TM2_HOST_DX_COM_H */
