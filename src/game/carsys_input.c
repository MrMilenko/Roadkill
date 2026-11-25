/*
 * carsys_input.c: PC input and render list helpers (see carsys_input.h).
 */
#include "carsys_input.h"

#include <stddef.h>

uint8_t carsys_input_device_type(const CarsysInput* in, int32_t device) {
    if (device == 1) return in->device_types[0];
    if (device == 2) return in->device_types[1];
    if (device == 4) return in->device_types[2];
    return 2;
}

uint8_t carsys_input_first_key(const CarsysInput* in) {
    for (int i = 0; i < 0x100; i++)
        if (in->keys[i]) return (uint8_t)i;
    return 0;
}

void carsys_world_model_add(const CarsysInput* in, void* model) {
    for (int i = 0; i < 20; i++)
        if (in->models[i] == NULL) {
            in->models[i] = model;
            return;
        }
}

void carsys_world_model_remove(const CarsysInput* in, void* model) {
    for (int i = 0; i < 20; i++)
        if (in->models[i] == model) {
            in->models[i] = NULL;
            return;
        }
}

uint8_t carsys_input_key(const CarsysInput* in) {
    return carsys_input_device_type(in, 1) == 1 ? carsys_input_first_key(in) : 0;
}

uint8_t carsys_input_mouse_button(const CarsysInput* in) {
    if (carsys_input_device_type(in, 2) != 1) return 0;
    if (in->button(in->user, in->mouse, 0)) return 5;
    if (in->button(in->user, in->mouse, 1)) return 6;
    if (in->button(in->user, in->mouse, 2)) return 7;
    return 0;
}
