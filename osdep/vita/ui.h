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

enum ui_key_code {
    UI_KEY_CODE_VITA_DPAD_LEFT,
    UI_KEY_CODE_VITA_DPAD_RIGHT,
    UI_KEY_CODE_VITA_DPAD_UP,
    UI_KEY_CODE_VITA_DPAD_DOWN,
    UI_KEY_CODE_VITA_ACTION_SQUARE,
    UI_KEY_CODE_VITA_ACTION_CIRCLE,
    UI_KEY_CODE_VITA_ACTION_TRIANGLE,
    UI_KEY_CODE_VITA_ACTION_CROSS,
    UI_KEY_CODE_VITA_L1,
    UI_KEY_CODE_VITA_R1,
    UI_KEY_CODE_VITA_START,
    UI_KEY_CODE_VITA_SELECT,
    UI_KEY_CODE_VITA_END,
};

enum ui_key_state {
    UI_KEY_STATE_DOWN,
    UI_KEY_STATE_UP,
};

struct ui_panel;
struct ui_texture;
struct ui_context;

struct ui_texture_draw_args {
    struct mp_rect *src;
    struct mp_rect *dst;
};

struct ui_panel_player_init_params {
    char *path;
};

struct ui_panel_player_vo_fns {
    void (*draw)(struct ui_context *ctx);
    void (*uninit)(struct ui_context *ctx);
    void (*send_key)(struct ui_context *ctx, enum ui_key_code key, enum ui_key_state state);
};

struct ui_context {
    void *priv_platform;
    void *priv_render;
    void *priv_panel;
    void *priv_context;
    struct mp_dispatch_queue *dispatch;
    const struct ui_panel *panel;
};

struct ui_panel {
    int priv_size;
    bool (*init)(struct ui_context *ctx, void *params);
    void (*uninit)(struct ui_context *ctx);
    void (*on_show)(struct ui_context *ctx);
    void (*on_hide)(struct ui_context *ctx);
    void (*on_draw)(struct ui_context *ctx);
    void (*on_poll)(struct ui_context *ctx);
    void (*on_key)(struct ui_context *ctx, enum ui_key_code code, enum ui_key_state state);
};

struct ui_platform_driver {
    int priv_size;
    bool (*init)(struct ui_context *ctx);
    void (*uninit)(struct ui_context *ctx);
    void (*poll_events)(struct ui_context *ctx);
    uint32_t (*poll_keys)(struct ui_context *ctx);
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

void ui_panel_common_wakeup(struct ui_context *ctx);
void ui_panel_common_invalidate(struct ui_context *ctx);
void *ui_panel_common_get_priv(struct ui_context *ctx, const struct ui_panel *panel);
void ui_panel_common_push(struct ui_context *ctx, const struct ui_panel *panel, void *data);
void ui_panel_common_pop(struct ui_context *ctx);
void ui_panel_common_pop_all(struct ui_context *ctx);

void *ui_panel_player_get_vo_data(struct ui_context *ctx);
void ui_panel_player_set_vo_data(struct ui_context *ctx, void *data);
void ui_panel_player_set_vo_fns(struct ui_context *ctx, const struct ui_panel_player_vo_fns *fns);

extern const struct ui_panel ui_panel_player;

extern const struct ui_platform_driver ui_platform_driver_vita;
extern const struct ui_render_driver ui_render_driver_vita;
