/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_OGC

#include "SDL.h"

#include "../../events/SDL_events_c.h"

#include "SDL_ogcevents_c.h"
#include "SDL_ogcmouse.h"
#include "SDL_ogcvideo.h"

#include <ogc/system.h>
#include <wiiuse/wpad.h>

/* These variables can be set from the handlers registered in SDL_main() */
bool OGC_PowerOffRequested = false;
bool OGC_ResetRequested = false;

#ifdef __wii__
#define MAX_WII_MOUSE_BUTTONS 2
static const struct {
    int wii;
    int mouse;
} s_mouse_button_map[MAX_WII_MOUSE_BUTTONS] = {
    { WPAD_BUTTON_B, SDL_BUTTON_LEFT },
    { WPAD_BUTTON_A, SDL_BUTTON_RIGHT },
};

static void pump_ir_events(_THIS)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    bool wiimote_pointed_at_screen = false;

    if (!_this->windows) return;

    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        /* Get events from WPAD; we don't need to do this if the joystick
         * system was initialized, because in that case this operation is done
         * there at every event loop iteration. */
        WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
    }

    for (int i = 0; i < 4; i++) {
        WPADData *data = WPAD_Data(i);

        if (!data->ir.valid) continue;

        wiimote_pointed_at_screen = true;
        SDL_SendMouseMotion(_this->windows, i,
                            0, data->ir.x, data->ir.y);

        for (int b = 0; b < MAX_WII_MOUSE_BUTTONS; b++) {
            if (data->btns_d & s_mouse_button_map[b].wii) {
                SDL_SendMouseButton(_this->windows, i,
                                    SDL_PRESSED, s_mouse_button_map[b].mouse);
            }
            if (data->btns_u & s_mouse_button_map[b].wii) {
                SDL_SendMouseButton(_this->windows, i,
                                    SDL_RELEASED, s_mouse_button_map[b].mouse);
            }
        }
    }

    /* Unfortunately SDL in practice supports only one mouse, so we are
     * consolidating all the wiimotes as a single device.
     * Here we check if any wiimote is pointed at the screen, in which case we
     * show the default cursor (the Wii hand); if not, then the default cursor
     * is hidden. Note that this only affects applications which haven't
     * explicitly set a cursor: the others remain in full control of whether a
     * cursor should be shown or not. */
    if (mouse && mouse->cur_cursor == mouse->def_cursor &&
        mouse->cursor_shown != wiimote_pointed_at_screen) {
        SDL_ShowCursor(wiimote_pointed_at_screen);
    }
}
#endif

void OGC_PumpEvents(_THIS)
{
    if (OGC_ResetRequested || OGC_PowerOffRequested) {
        SDL_Event ev;
        ev.type = SDL_QUIT;
        SDL_PushEvent(&ev);
        if (OGC_PowerOffRequested) {
            SYS_ResetSystem(SYS_POWEROFF, 0, 0);
        }
    }

#ifdef __wii__
    pump_ir_events(_this);
#endif
}

#endif /* SDL_VIDEO_DRIVER_OGC */

/* vi: set ts=4 sw=4 expandtab: */
