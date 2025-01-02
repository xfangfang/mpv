/* Copyright (C) 2018 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MPV_CLIENT_API_RENDER_GL_H_
#define MPV_CLIENT_API_RENDER_GL_H_

#include <psp2/gxm.h>

#include "render.h"
#include "nanovg_gxm_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * For initializing the mpv deko3d state via MPV_RENDER_PARAM_DEKO3D_INIT_PARAMS.
 */
typedef struct mpv_gxm_init_params {
    /**
     * The gxm context that will be used in subsequent operation.
     */
    SceGxmContext *context;
    SceGxmShaderPatcher *shader_patcher;
    int buffer_index;
} mpv_gxm_init_params;

/**
 * For MPV_RENDER_PARAM_GXM_FBO.
 */
typedef struct mpv_gxm_fbo {
    /**
     * Texture object.
     */
    NVGXMframebuffer *tex;
    /**
     * Valid dimensions. This must refer to the size of the framebuffer. This
     * must always be set.
     */
    int w, h;
    /**
     * Underlying texture internal format. This must always be set.
     */
    SceGxmTextureFormat format;
} mpv_gxm_fbo;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif