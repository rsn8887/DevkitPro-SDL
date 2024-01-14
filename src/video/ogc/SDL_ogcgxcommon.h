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

#ifndef SDL_ogcgxcommon_h_
#define SDL_ogcgxcommon_h_

#include <gctypes.h>

#define GX_COLOR_AS_U32(c) *((u32*)&c)

void OGC_draw_init(int w, int h, int h_aspect, int v_aspect);
void OGC_set_viewport(int x, int y, int w, int h, float h_aspect, float v_aspect);
void OGC_load_texture(void *texels, int w, int h, u8 gx_format);

#endif /* SDL_ogcgxcommon_h_ */

/* vi: set ts=4 sw=4 expandtab: */
