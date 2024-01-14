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

#ifdef SDL_VIDEO_RENDER_OGC

#include "../SDL_sysrender.h"
#include "SDL_hints.h"

#include "../../video/ogc/SDL_ogcgxcommon.h"
#include "../../video/ogc/SDL_ogcpixels.h"
#include "../../video/ogc/SDL_ogcvideo.h"

#include <malloc.h>
#include <ogc/gx.h>
#include <ogc/video.h>

/* Size of Persistent drawbuffer (Single Buffered) */
#define RENDER_QUEUE_PER_POOLSIZE 1024 * 256 // 256K of persistent renderqueue
/* Size of Oneshot drawbuffer (Double Buffered, so it uses this size * 2) */
#define RENDER_QUEUE_OS_POOLSIZE 1024 * 1024 * 2 // 2048K of oneshot renderqueue

typedef struct
{
    u32 drawColor;
} OGC_RenderData;

typedef struct
{
    void *texels;
    u16 pitch;
    u8 format;
} OGC_TextureData;

static void OGC_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
}

static int OGC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    u32 texture_size;
    OGC_TextureData *ogc_tex;

    ogc_tex = SDL_calloc(1, sizeof(OGC_TextureData));
    if (!ogc_tex) {
        return SDL_OutOfMemory();
    }

    ogc_tex->format = OGC_texture_format_from_SDL(texture->format);
    texture_size = GX_GetTexBufferSize(texture->w, texture->h, ogc_tex->format,
                                       GX_FALSE, 0);
    ogc_tex->texels = memalign(32, texture_size);
    if (!ogc_tex->texels) {
        SDL_free(ogc_tex);
        return SDL_OutOfMemory();
    }

    texture->driverdata = ogc_tex;
    return 0;
}

static int OGC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                           const SDL_Rect *rect, void **pixels, int *pitch)
{
    // TODO
    return 0;
}

static void OGC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    // TODO
}

static int OGC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                             const SDL_Rect *rect, const void *pixels, int pitch)
{
    OGC_TextureData *ogc_tex = texture->driverdata;
    u8 format;

    // TODO: take rect into account
    OGC_pixels_to_texture((void*)pixels, texture->format, texture->w, texture->h,
                          pitch, ogc_tex->texels, &format);

    return 0;
}

static void OGC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
    // TODO
}

static int OGC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return 0;
}

static int OGC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return 0; /* nothing to do in this backend. */
}

static int OGC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                               const SDL_FPoint *points, int count)
{
    size_t size = count * sizeof(SDL_FPoint);
    SDL_FPoint *vertices = SDL_AllocateRenderVertices(renderer, size,
                                                      4, &cmd->data.draw.first);
    if (!vertices) {
        return -1;
    }

    cmd->data.draw.count = count;
    SDL_memcpy(vertices, points, size);
    return 0;
}

static int OGC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                               const SDL_FRect *rects, int count)
{
    size_t size = count * sizeof(SDL_FPoint) * 4;
    SDL_FPoint *vertices = SDL_AllocateRenderVertices(renderer, size,
                                                      4, &cmd->data.draw.first);
    if (!vertices) {
        return -1;
    }

    cmd->data.draw.count = count;
    for (int i = 0; i < count; i++) {
        vertices[i].x = rects[i].x;
        vertices[i].y = rects[i].y;
        vertices[i+1].x = rects[i].x + rects[i].w;
        vertices[i+1].y = rects[i].y;
        vertices[i+2].x = rects[i].x + rects[i].w;
        vertices[i+2].y = rects[i].y + rects[i].h;
        vertices[i+3].x = rects[i].x;
        vertices[i+3].y = rects[i].y + rects[i].h;
    }
    return 0;
}

static int OGC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                             const float *xy, int xy_stride, const SDL_Color *color, int color_stride,
                             const float *uv, int uv_stride,
                             int num_vertices, const void *indices, int num_indices, int size_indices,
                             float scale_x, float scale_y)
{
    int i;
    int count = indices ? num_indices : num_vertices;
    size_t size_per_element;
    char *vertices;

    cmd->data.draw.count = count;
    size_indices = indices ? size_indices : 0;

    size_per_element = sizeof(SDL_FPoint) + sizeof(SDL_Color);
    if (texture) {
        size_per_element += sizeof(SDL_FPoint);
    }

    vertices = SDL_AllocateRenderVertices(renderer, count * size_per_element,
                                          4, &cmd->data.draw.first);
    if (!vertices) {
        return -1;
    }

    for (i = 0; i < count; i++) {
        int j;
        float *xy_;
        float *uv_;
        SDL_Color col;
        char *vertex;
        SDL_FPoint *vertex_xy;
        if (size_indices == 4) {
            j = ((const Uint32 *)indices)[i];
        } else if (size_indices == 2) {
            j = ((const Uint16 *)indices)[i];
        } else if (size_indices == 1) {
            j = ((const Uint8 *)indices)[i];
        } else {
            j = i;
        }

        xy_ = (float *)((char *)xy + j * xy_stride);
        col = *(SDL_Color *)((char *)color + j * color_stride);
        uv_ = (float *)((char *)uv + j * uv_stride);

        vertex = vertices + size_per_element * i;

        vertex_xy = (SDL_FPoint *)vertex;
        vertex_xy->x = xy_[0];
        vertex_xy->y = xy_[1];

        *(SDL_Color *)(vertex + sizeof(SDL_FPoint)) = col;

        if (texture) {
            SDL_FPoint *vertex_uv = (SDL_FPoint *)(vertex + sizeof(SDL_FPoint) + sizeof(SDL_Color));
            vertex_uv->x = uv_[0];
            vertex_uv->y = uv_[1];
        }
    }

    return 0;
}

static int OGC_RenderSetViewPort(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    // TODO
    return 0;
}

static int OGC_RenderSetClipRect(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    // TODO
    return 0;
}

static int OGC_RenderSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    // TODO
    return 0;
}

static int OGC_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    // TODO
    return 0;
}

static void OGC_SetBlendMode(SDL_Renderer *renderer, int blendMode)
{
    // TODO
}

static int OGC_RenderGeometry(SDL_Renderer *renderer, void *vertices,
                              SDL_RenderCommand *cmd)
{
    const size_t count = cmd->data.draw.count;
    SDL_Texture *texture = cmd->data.draw.texture;
    size_t size_per_element;

    OGC_SetBlendMode(renderer, cmd->data.draw.blend);

    size_per_element = sizeof(SDL_FPoint) + sizeof(SDL_Color);
    if (texture) {
        size_per_element += sizeof(SDL_FPoint);
        OGC_TextureData *ogc_tex = texture->driverdata;
        OGC_load_texture(ogc_tex->texels, texture->w, texture->h,
                         ogc_tex->format);
    }

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    if (texture) {
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GX_SetNumTexGens(1);
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0,
                       GX_DF_NONE, GX_AF_NONE);

        GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    } else {
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    }

    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, count);
    for (int i = 0; i < count; i++) {
        void *vertex = vertices + cmd->data.draw.first + size_per_element * i;
        SDL_FPoint *vertex_xy = vertex;
        SDL_Color *c = (SDL_Color*)(vertex + sizeof(SDL_FPoint));

        GX_Position2f32(vertex_xy->x, vertex_xy->y);
        GX_Color4u8(c->r, c->g, c->b, c->a);
        if (texture) {
            SDL_FPoint *vertex_uv = (SDL_FPoint *)(vertex + sizeof(SDL_FPoint) + sizeof(SDL_Color));
            GX_TexCoord2f32(vertex_uv->x, vertex_uv->y);
        }
    }
    GX_End();
    return 0;
}

int OGC_RenderPrimitive(SDL_Renderer *renderer, u8 primitive,
                        void *vertices, SDL_RenderCommand *cmd)
{
    size_t count = cmd->data.draw.count;
    const SDL_FPoint *verts = (SDL_FPoint *)(vertices + cmd->data.draw.first);
    GXColor c = {
        cmd->data.draw.r,
        cmd->data.draw.g,
        cmd->data.draw.b,
        cmd->data.draw.a
    };

    OGC_SetBlendMode(renderer, cmd->data.draw.blend);

    /* TODO: optimize state changes. */
    GX_SetTevColor(GX_TEVREG0, c);
    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
    GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);

    if (primitive == GX_QUADS) count *= 4;

    GX_Begin(primitive, GX_VTXFMT0, count);
    for (int i = 0; i < count; i++) {
        GX_Position2f32(verts[i].x, verts[i].y);
    }
    GX_End();

    return 0;
}

static int OGC_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_SETVIEWPORT:
            OGC_RenderSetViewPort(renderer, cmd);
            break;
        case SDL_RENDERCMD_SETCLIPRECT:
            OGC_RenderSetClipRect(renderer, cmd);
            break;
        case SDL_RENDERCMD_SETDRAWCOLOR:
            OGC_RenderSetDrawColor(renderer, cmd);
            break;
        case SDL_RENDERCMD_CLEAR:
            OGC_RenderClear(renderer, cmd);
            break;
        case SDL_RENDERCMD_DRAW_POINTS:
            OGC_RenderPrimitive(renderer, GX_POINTS, vertices, cmd);
            break;
        case SDL_RENDERCMD_DRAW_LINES:
            OGC_RenderPrimitive(renderer, GX_LINESTRIP, vertices, cmd);
            break;
        case SDL_RENDERCMD_FILL_RECTS:
            OGC_RenderPrimitive(renderer, GX_QUADS, vertices, cmd);
            break;
        case SDL_RENDERCMD_COPY: /* unused */
            break;
        case SDL_RENDERCMD_COPY_EX: /* unused */
            break;
        case SDL_RENDERCMD_GEOMETRY:
            OGC_RenderGeometry(renderer, vertices, cmd);
            break;
        case SDL_RENDERCMD_NO_OP:
            break;
        }
        cmd = cmd->next;
    }
    return 0;
}

static int OGC_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                                Uint32 format, void *pixels, int pitch)
{
    return SDL_Unsupported();
}

static int OGC_RenderPresent(SDL_Renderer *renderer)
{
    GX_DrawDone();

    OGC_VideoFlip(renderer->window);
    return 0;
}

static void OGC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    OGC_TextureData *ogc_tex = texture->driverdata;

    if (ogc_tex) {
        free(ogc_tex->texels);
        SDL_free(ogc_tex);
        texture->driverdata = NULL;
    }
}

static void OGC_DestroyRenderer(SDL_Renderer *renderer)
{
    OGC_RenderData *data = renderer->driverdata;

    if (data) {
        SDL_free(data);
    }

    SDL_free(renderer);
}

static int OGC_SetVSync(SDL_Renderer *renderer, const int vsync)
{
    return 0;
}

static SDL_Renderer *OGC_CreateRenderer(SDL_Window *window, Uint32 flags)
{
    SDL_Renderer *renderer;
    OGC_RenderData *data;
    SDL_bool dynamicVsync;

    renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (OGC_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        OGC_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    renderer->WindowEvent = OGC_WindowEvent;
    renderer->CreateTexture = OGC_CreateTexture;
    renderer->UpdateTexture = OGC_UpdateTexture;
    renderer->LockTexture = OGC_LockTexture;
    renderer->UnlockTexture = OGC_UnlockTexture;
    renderer->SetTextureScaleMode = OGC_SetTextureScaleMode;
    renderer->SetRenderTarget = OGC_SetRenderTarget;
    renderer->QueueSetViewport = OGC_QueueSetViewport;
    renderer->QueueSetDrawColor = OGC_QueueSetViewport;
    renderer->QueueDrawPoints = OGC_QueueDrawPoints;
    renderer->QueueDrawLines = OGC_QueueDrawPoints;
    renderer->QueueFillRects = OGC_QueueFillRects;
    renderer->QueueGeometry = OGC_QueueGeometry;
    renderer->RunCommandQueue = OGC_RunCommandQueue;
    renderer->RenderReadPixels = OGC_RenderReadPixels;
    renderer->RenderPresent = OGC_RenderPresent;
    renderer->DestroyTexture = OGC_DestroyTexture;
    renderer->DestroyRenderer = OGC_DestroyRenderer;
    renderer->SetVSync = OGC_SetVSync;
    renderer->info = OGC_RenderDriver.info;
    renderer->driverdata = data;
    renderer->window = window;

    return renderer;
}

SDL_RenderDriver OGC_RenderDriver = {
    .CreateRenderer = OGC_CreateRenderer,
    .info = {
        .name = "OGC renderer",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 2,
        .texture_formats = {
            [0] = SDL_PIXELFORMAT_RGB565,
            [1] = SDL_PIXELFORMAT_RGBA8888,
            // TODO: add more
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024,
    }
};

#endif /* SDL_VIDEO_RENDER_OGC */

/* vi: set ts=4 sw=4 expandtab: */
