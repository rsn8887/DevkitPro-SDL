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

#include "SDL_ogcgxcommon.h"
#include "SDL_ogcpixels.h"

#include <ogc/cache.h>
#include <ogc/gx.h>
#include <ogc/video.h>

typedef struct tagcamera {
    guVector pos;
    guVector up;
    guVector view;
} camera;

static camera cam = {
    { 0.0F, 0.0F, 0.0F },
    { 0.0F, -0.5F, 0.0F },
    { 0.0F, 0.0F, 0.5F }
};

static const f32 tex_pos[] __attribute__((aligned(32))) = {
    0.0,
    0.0,
    1.0,
    0.0,
    1.0,
    1.0,
    0.0,
    1.0,
};

void OGC_set_viewport(int x, int y, int w, int h, float h_aspect, float v_aspect)
{
    Mtx m, mv, view;
    Mtx44 proj;

    GX_SetViewport(x, y, w, h, 0, 1);
    GX_SetScissor(x, y, w, h);

    memset(&view, 0, sizeof(Mtx));
    guLookAt(view, &cam.pos, &cam.up, &cam.view);
    guMtxIdentity(m);
    guMtxTransApply(m, m, 0.5 + (-w / 2.0), 0.5 + (-h / 2.0), 1000);
    guMtxConcat(view, m, mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);

    // matrix, t, b, l, r, n, f
    guOrtho(proj, v_aspect, -v_aspect, -h_aspect, h_aspect, 100, 1000);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void OGC_draw_init(int w, int h, int h_aspect, int v_aspect)
{
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OGC_draw_init called with %d, %d", w, h);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);

    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GX_SetArray(GX_VA_TEX0, (void *)tex_pos, 2 * sizeof(f32));
    GX_SetNumTexGens(1);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0,
                   GX_DF_NONE, GX_AF_NONE);

    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    OGC_set_viewport(0, 0, w, h, h_aspect, v_aspect);

    GX_InvVtxCache(); // update vertex cache
}

void OGC_load_texture(void *texels, int w, int h, u8 format)
{
    GXTexObj texobj_a, texobj_b;

    if (format == GX_TF_CI8) {
        GX_InitTexObjCI(&texobj_a, texels, w, h, GX_TF_CI8, GX_CLAMP, GX_CLAMP, 0, GX_TLUT0);
        GX_InitTexObjCI(&texobj_b, texels, w, h, GX_TF_CI8, GX_CLAMP, GX_CLAMP, 0, GX_TLUT1);
        GX_LoadTexObj(&texobj_b, GX_TEXMAP1);

        // Setup TEV to combine Red+Green and Blue paletted images
        GX_SetTevColor(GX_TEVREG0, (GXColor){ 255, 255, 0, 0 });
        GX_SetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_ALPHA, GX_CH_BLUE, GX_CH_ALPHA);
        GX_SetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_ALPHA, GX_CH_ALPHA, GX_CH_BLUE, GX_CH_ALPHA);
        // first stage = red and green
        GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
        // second stage = add blue (and opaque alpha)
        GX_SetTevOp(GX_TEVSTAGE1, GX_BLEND);
        GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLORNULL);
        GX_SetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP2);
        GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

        GX_SetNumTevStages(2);
    } else {
        GX_InitTexObj(&texobj_a, texels, w, h, format, GX_CLAMP, GX_CLAMP, GX_FALSE);
    }

    GX_InitTexObjLOD(&texobj_a, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    GX_LoadTexObj(&texobj_a, GX_TEXMAP0); // load texture object so its ready to use
}

#endif /* SDL_VIDEO_DRIVER_OGC */

/* vi: set ts=4 sw=4 expandtab: */
