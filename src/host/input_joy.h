/*
 * input_joy.h: the game controllers behind DirectInput's joystick devices (port code, host only;
 * not from the original). input_joy.c has the SDL side and the button / axis mapping;
 * input_dinput.c the COM objects the game sees.
 */
#ifndef TM2_HOST_INPUT_JOY_H
#define TM2_HOST_INPUT_JOY_H

#include <stdint.h>

#define JOY_MAX 16

/* the logical DirectInput axes of a device, in DIJOYSTATE order */
enum { JOY_X, JOY_Y, JOY_Z, JOY_RX, JOY_RY, JOY_RZ, JOY_S0, JOY_S1, JOY_NAXES };

typedef struct JoyInfo {
    char name[64];                /* product name (also the instance name, numbered when repeated) */
    char instance[64];
    uint8_t guid_instance[16];    /* stable: from SDL's device GUID and the copy number of that GUID */
    uint8_t guid_product[16];     /* {PIDVID-0000-0000-0000-504944564944} as DirectInput makes it */
    int gamepad;                  /* 1: an SDL GameController (mapped layout), 0: a raw joystick */
    int has_axis[JOY_NAXES];      /* which logical axes the device has */
    int nbuttons;                 /* <= 32 */
    int npovs;                    /* 0 or 1 */
} JoyInfo;

typedef struct JoyRaw {
    int32_t axis[JOY_NAXES];      /* -32768..32767 around 0 (triggers: 0..32767, rest 0) */
    uint32_t pov;                 /* hundredths of degrees clockwise from up, 0xffffffff centred */
    uint8_t button[32];           /* 1 = down */
} JoyRaw;

/* the controllers (opened at the first call; TM2_NOJOY=1: none; TM2_VIRTUAL_PAD=1 attaches a
 * scripted virtual one first). The list does not change afterwards (no hot plug). */
int joy_count(void);
const JoyInfo* joy_info(int i);
/* the current state of controller i (pumps SDL's joystick state; runs the virtual pad's script) */
int joy_read(int i, JoyRaw* out);

#endif /* TM2_HOST_INPUT_JOY_H */
