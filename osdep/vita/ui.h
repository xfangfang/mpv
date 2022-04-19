#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "misc/dispatch.h"

#define VITA_SCREEN_W 960
#define VITA_SCREEN_H 544

enum ui_texure_fmt {
    TEX_FMT_UNKNOWN,
    TEX_FMT_RGBA,
    TEX_FMT_YUV420,
};

struct ui_texture;

struct ui_context_internal;

struct ui_texture_draw_args {
    struct mp_rect *src;
    struct mp_rect *dst;
};

struct ui_context {
    void *priv_platform;
    void *priv_render;
    struct ui_context_internal *internal;
    struct mp_dispatch_queue *dispatch;

    void *video_ctx;
    void (*video_draw_cb)(struct ui_context *ctx);
    void (*video_uninit_cb)(struct ui_context *ctx);
};

struct ui_platform_driver {
    int priv_size;
    bool (*init)(struct ui_context *ctx);
    void (*uninit)(struct ui_context *ctx);
    void (*poll_events)(struct ui_context *ctx);
};

struct ui_render_driver {
    int priv_size;

    bool (*init)(struct ui_context *ctx);
    void (*uninit)(struct ui_context *ctx);

    void (*render_start)(struct ui_context *ctx);
    void (*render_end)(struct ui_context *ctx);

    bool (*texture_init)(struct ui_context *ctx, struct ui_texture **tex,
                         enum ui_texure_fmt fmt, int w, int h);
    void (*texture_uninit)(struct ui_context *ctx, struct ui_texture **tex);
    void (*texture_upload)(struct ui_context *ctx, struct ui_texture *tex,
                           void **data, int *strides, int planes);

    void (*draw_texture)(struct ui_context *ctx, struct ui_texture *tex,
                         struct ui_texture_draw_args *args);
};

void ui_request_redraw(struct ui_context *ctx);
void ui_request_mpv_shutdown(struct ui_context* ctx);

extern const struct ui_platform_driver ui_platform_driver_vita;
extern const struct ui_render_driver ui_render_driver_vita;
