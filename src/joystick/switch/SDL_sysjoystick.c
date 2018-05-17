/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_JOYSTICK_SWITCH

/* This is the dummy implementation of the SDL joystick API */

#include "SDL_events.h"
#include "../SDL_sysjoystick.h"

#include <switch.h>

#define JOYSTICK_COUNT 8

typedef struct JoystickState
{
    HidControllerID id;
    JoystickPosition l_pos;
    JoystickPosition r_pos;
    u64 buttons;
} JoystickState;

/* Current pad state */
static JoystickState pad[JOYSTICK_COUNT];

static HidControllerID pad_id[JOYSTICK_COUNT] = {
    CONTROLLER_P1_AUTO, CONTROLLER_PLAYER_2,
    CONTROLLER_PLAYER_3, CONTROLLER_PLAYER_4,
    CONTROLLER_PLAYER_5, CONTROLLER_PLAYER_6,
    CONTROLLER_PLAYER_7, CONTROLLER_PLAYER_8
};

static const HidControllerKeys pad_mapping[] = {
    KEY_A, KEY_B, KEY_X, KEY_Y,
    KEY_LSTICK, KEY_RSTICK,
    KEY_L, KEY_R,
    KEY_ZL, KEY_ZR,
    KEY_PLUS, KEY_MINUS,
    KEY_DLEFT, KEY_DUP, KEY_DRIGHT, KEY_DDOWN,
    KEY_LSTICK_LEFT, KEY_LSTICK_UP, KEY_LSTICK_RIGHT, KEY_LSTICK_DOWN,
    KEY_RSTICK_LEFT, KEY_RSTICK_UP, KEY_RSTICK_RIGHT, KEY_RSTICK_DOWN
};

/* Function to scan the system for joysticks.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    for (int i = 0; i < JOYSTICK_COUNT; i++) {
        pad[i].id = pad_id[i];
    }

    return JOYSTICK_COUNT;
}

int
SDL_SYS_NumJoysticks(void)
{
    return JOYSTICK_COUNT;
}

void
SDL_SYS_JoystickDetect(void)
{
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    return "Switch Controller";
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    return device_index;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    joystick->nbuttons = sizeof(pad_mapping) / sizeof(*pad_mapping);
    joystick->naxes = 4;
    joystick->nhats = 0;
    joystick->instance_id = device_index;

    return 0;
}

/* Function to determine if this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
    return SDL_TRUE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{
    u64 changed;
    static JoystickState pad_old[JOYSTICK_COUNT];

    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > JOYSTICK_COUNT) {
        return;
    }

    hidJoystickRead(&pad[index].l_pos, pad[index].id, JOYSTICK_LEFT);
    hidJoystickRead(&pad[index].r_pos, pad[index].id, JOYSTICK_RIGHT);
    pad[index].buttons = hidKeysHeld(pad[index].id);

    // Axes
    if (pad_old[index].l_pos.dx != pad[index].l_pos.dx) {
        SDL_PrivateJoystickAxis(joystick, 0, (Sint16) pad[index].l_pos.dx);
        pad_old[index].l_pos.dx = pad[index].l_pos.dx;
    }
    if (pad_old[index].l_pos.dy != pad[index].l_pos.dy) {
        SDL_PrivateJoystickAxis(joystick, 1, (Sint16) -pad[index].l_pos.dy);
        pad_old[index].l_pos.dy = -pad[index].l_pos.dy;
    }
    if (pad_old[index].r_pos.dx != pad[index].r_pos.dx) {
        SDL_PrivateJoystickAxis(joystick, 2, (Sint16) pad[index].r_pos.dx);
        pad_old[index].r_pos.dx = pad[index].r_pos.dx;
    }
    if (pad_old[index].r_pos.dy != pad[index].r_pos.dy) {
        SDL_PrivateJoystickAxis(joystick, 3, (Sint16) -pad[index].r_pos.dy);
        pad_old[index].r_pos.dy = -pad[index].r_pos.dy;
    }

    // Buttons
    changed = pad_old[index].buttons ^ pad[index].buttons;
    pad_old[index].buttons = pad[index].buttons;
    if (changed) {
        for (int i = 0; i < joystick->nbuttons; i++) {
            if (changed & pad_mapping[i]) {
                SDL_PrivateJoystickButton(
                    joystick, (Uint8) i,
                    (Uint8) ((pad[index].buttons & pad_mapping[i]) ? SDL_PRESSED : SDL_RELEASED));
            }
        }
    }
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = SDL_SYS_JoystickNameForDeviceIndex(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick *joystick)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = joystick->name;
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

#endif /* SDL_JOYSTICK_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
