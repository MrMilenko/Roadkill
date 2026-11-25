/*
 * pf3_ddinit.c: TM2.EXE 0x004b3d80 ddraw_init, ported over Pf2Env (pf2_env.h): the DirectDraw
 * (re)creation and display mode enumeration of the display object (docs/PLATFORM.md Part A).
 * Emulator diff: tools/emu/check_pf3_ddinit.sh.
 *
 * Display object D fields: +0x04 IDirectDraw (PF2_IUNK here), +0x08 IDirectDraw2, +0x24 IDirect3D2,
 * +0x30 a 3D object (PF2_IUNK), +0x34 3D device GUID, +0x50 "Renderer" registry value, +0x54 HWND,
 * +0x70 full screen, +0x78 client rect, +0x88 desktop bits per pixel. Globals: 0x004e86ac last
 * SetCooperativeLevel flags, 0x00c85008 hardware 3D on, 0x00c848e0 mode list.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define COOP 0x004e86acu
#define HW3D 0x00c85008u
#define MODES 0x00c848e0u

#define I_GetClientRect 0x00c904bcu
#define I_ClientToScreen 0x00c90470u
#define I_GetDC 0x00c904c4u
#define I_GetSystemMetrics 0x00c904e8u
#define I_GetDeviceCaps 0x00c901e4u
#define I_ReleaseDC 0x00c90544u

#define F_MAIN_HWND 0x004a6dccu        /* (eax 0x00bdfcd0) -> HWND */
#define F_REG_INIT 0x004ac4b0u         /* registry object init (eax obj, edx 0, ebx 0x10) */
#define F_REG_OPEN 0x004ac528u         /* (eax obj, edx root, ebx 0) -> 0 on success */
#define F_REG_READ_DATA 0x004ac5e4u    /* (eax obj, edx name, ebx buffer, ecx &size) */
#define F_REG_READ_INT 0x004ac5bcu     /* (eax obj, edx name, ebx out) */
#define F_REG_CLOSE 0x004ac590u        /* (eax obj) */
#define F_D3D_RELEASE_DEVICE 0x004b5ee8u
#define F_RELEASE_SURFACES 0x004b4374u
#define F_DIRECTDRAWCREATE 0x004d18eau /* import thunk: DirectDrawCreate(guid, &dd, NULL), stdcall */
#define F_ERROR_BOX 0x004bc660u        /* cdecl (code[, hr & 0xfff]) */
#define F_QSORT 0x004c1132u            /* (eax base, edx count, ebx size, ecx compare) */
#define F_GET_CAPS 0x004b5874u
#define F_D3D_INIT 0x004b5bf0u

/* SetCooperativeLevel(hwnd, flags) unless the cache 0x004e86ac has it (then 0) */
static uint32_t coop(Pf2Env* e, uint32_t d, uint32_t dd, uint32_t flags) {
    if (R32(e, COOP) == flags) return 0;
    W32(e, COOP, flags);
    return PF2_C2(e, PF2_IDD2, 0x50, dd, R32(e, d + 0x54), flags);
}

/* DirectDrawCreate(guid, &+4, NULL); on success (+0x08 is 0 here, so the original's cooperative
   level call in between never runs) QueryInterface(IID_IDirectDraw2 0x004e54e4, &+8) and, the
   IDirectDraw set, Release it (+4 = 0). ORIGINAL BUG kept: the result taken on is Release's (the
   reference count), not QueryInterface's, whenever +4 is set. */
static uint32_t create(Pf2Env* e, uint32_t d, uint32_t guid) {
    uint32_t r = pf2_call3(e, F_DIRECTDRAWCREATE, guid, d + 4, 0);
    if (r != 0) return r;
    uint32_t dd = R32(e, d + 8);
    if (dd) r = coop(e, d, dd, 8);
    if (r != 0) return r;
    r = PF2_C2(e, PF2_IUNK, 0, R32(e, d + 4), 0x004e54e4u, d + 8);
    uint32_t dd1 = R32(e, d + 4);
    if (dd1) {
        r = PF2_C0(e, PF2_IUNK, 8, dd1);
        W32(e, d + 4, 0);
    }
    return r;
}

/* 0x004b3d80 ddraw_init(eax D, edx want_3d). Locals (in scratch): +0x00 GUID (16), +0x10 the
   registry object, +0x14 its size argument. Returns 0 or the failing HRESULT. */
uint32_t pf3_ddraw_init(Pf2Env* e, uint32_t d, uint32_t want) {
    uint32_t guid = e->scratch, reg = e->scratch + 0x10, size = e->scratch + 0x14;
    if (R32(e, d + 8) == 0) {
        uint32_t hwnd = pf2_call1(e, F_MAIN_HWND, 0x00bdfcd0u);
        W32(e, d + 0x54, hwnd);
        pf2_call2(e, I_GetClientRect, hwnd, d + 0x78);
        pf2_call2(e, I_ClientToScreen, R32(e, d + 0x54), d + 0x78);
        pf2_call2(e, I_ClientToScreen, R32(e, d + 0x54), d + 0x80);
        uint32_t hdc = pf2_call1(e, I_GetDC, R32(e, d + 0x54));
        pf2_call1(e, I_GetSystemMetrics, 0);
        pf2_call1(e, I_GetSystemMetrics, 1);
        W32(e, d + 0x88, pf2_call2(e, I_GetDeviceCaps, hdc, 12));
        pf2_call2(e, I_ReleaseDC, R32(e, d + 0x54), hdc);
    }
    pf2_call3(e, F_REG_INIT, reg, 0, 0x10);
    pf2_zero(e, guid, 0x10);
    pf2_zero(e, d + 0x34, 0x10);
    uint32_t gp = guid;
    W32(e, d + 0x50, 0);
    if (pf2_call3(e, F_REG_OPEN, reg, 0x80000002u, 0) == 0) {
        W32(e, size, 0x10);
        pf2_call4(e, F_REG_READ_DATA, reg, 0x004e3684u, guid, size);   /* "DirectDraw GUID" */
        pf2_call3(e, F_REG_READ_INT, reg, 0x004e3694u, d + 0x50);      /* "Renderer" */
        pf2_call1(e, F_REG_CLOSE, reg);
    }
    if (want == 1 && R32(e, d + 0x50) == 0 && R32(e, d + 8) != 0) return 0;

    /* release the old objects */
    if (R32(e, d + 8)) {
        pf2_call1(e, F_D3D_RELEASE_DEVICE, d);
        if (R32(e, d + 0x30)) {
            PF2_C0(e, PF2_IUNK, 8, R32(e, d + 0x30));
            W32(e, d + 0x30, 0);
        }
        if (R32(e, d + 0x24)) {
            PF2_C0(e, PF2_ID3D2, 8, R32(e, d + 0x24));
            W32(e, d + 0x24, 0);
        }
    }
    if (R32(e, d + 8)) coop(e, d, R32(e, d + 8), 8);
    pf2_call1(e, F_RELEASE_SURFACES, d);
    if (R32(e, d + 8)) PF2_C0(e, PF2_IDD2, 0x4c, R32(e, d + 8));
    if (R32(e, d + 8)) {
        PF2_C0(e, PF2_IDD2, 8, R32(e, d + 8));
        W32(e, d + 8, 0);
    }
    if (R32(e, d + 4)) {
        PF2_C0(e, PF2_IUNK, 8, R32(e, d + 4));
        W32(e, d + 4, 0);
    }

    /* +0x34 = IID_IDirect3DHALDevice (0x004e5594, 16 bytes) */
    memcpy(e->mem(e->user, d + 0x34), e->mem(e->user, 0x004e5594u), 16);
    /* ORIGINAL BUG: the "GUID is all zero" loop tests the first byte 16 times */
    if (pf2_r8(e, guid) == 0) gp = 0;
    if (want == 1 && want == R32(e, d + 0x50)) {
        W32(e, HW3D, 1);
        W32(e, d + 0x70, 1);
    } else {
        W32(e, HW3D, 0);
    }
    if (R32(e, HW3D) == 0) gp = 0;

    if (R32(e, d + 8) == 0) {
        uint32_t r = create(e, d, gp);
        if (r != 0 && gp != 0) r = create(e, d, 0);
        if (r == 0x887600deu) {            /* DDERR_NODIRECTDRAWSUPPORT */
            pf2_call1(e, F_ERROR_BOX, 0x13);
            return r;
        }
        if (r == 0x88760233u) {            /* DDERR_NODIRECTDRAWHW */
            pf2_call1(e, F_ERROR_BOX, 0x14);
            return r;
        }
        if (r != 0) {
            pf2_call2(e, F_ERROR_BOX, 9, r & 0xfff);
            return r;
        }
    }

    /* the mode list */
    W32(e, MODES, 0);
    uint32_t dd = R32(e, d + 8);
    if (dd) coop(e, d, dd, 0x17);
    PF2_C4(e, PF2_IDD2, 0x20, dd, 1, 0, MODES, 0x004b592cu);
    if (R32(e, d + 8)) coop(e, d, R32(e, d + 8), 8);
    pf2_call4(e, F_QSORT, MODES + 4, R32(e, MODES), 0x10, 0x004b584cu);
    pf2_call2(e, F_GET_CAPS, d, MODES);
    if (R32(e, HW3D)) pf2_call1(e, F_D3D_INIT, d);
    return 0;
}
