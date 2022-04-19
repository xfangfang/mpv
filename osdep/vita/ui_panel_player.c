#include "ui.h"
#include "ta/ta_talloc.h"
#include "libmpv/client.h"

struct priv_panel {
    mpv_handle *mpv;
    void *vo_data;
    ui_context_fn vo_draw_fn;
    ui_context_fn vo_uninit_fn;
};

void *ui_panel_player_get_vo_data(struct ui_context *ctx)
{
    struct priv_panel *priv = ui_panel_common_get_priv(ctx, &ui_panel_player);
    return priv ? priv->vo_data : NULL;
}

void ui_panel_player_set_vo_data(struct ui_context *ctx, void *data)
{
    struct priv_panel *priv = ui_panel_common_get_priv(ctx, &ui_panel_player);
    if (priv)
        priv->vo_data = ta_steal(priv, data);
}

void ui_panel_player_set_vo_draw_fn(struct ui_context *ctx, ui_context_fn f)
{
    struct priv_panel *priv = ui_panel_common_get_priv(ctx, &ui_panel_player);
    if (priv)
        priv->vo_draw_fn = f;
}

void ui_panel_player_set_vo_uninit_fn(struct ui_context *ctx, ui_context_fn f)
{
    struct priv_panel *priv = ui_panel_common_get_priv(ctx, &ui_panel_player);
    if (priv)
        priv->vo_uninit_fn = f;
}

static void on_mpv_wakeup(void *p)
{
    ui_panel_common_wakeup(p);
}

static bool player_init(struct ui_context *ctx, void *p)
{
    struct priv_panel *priv = ctx->priv_panel;
    priv->mpv = mpv_create();
    if (!priv->mpv)
        return false;

    mpv_set_option(priv->mpv, "wid", MPV_FORMAT_INT64, &ctx);
    mpv_set_option_string(priv->mpv, "idle", "yes");
    mpv_set_wakeup_callback(priv->mpv, on_mpv_wakeup, ctx);
    if (mpv_initialize(priv->mpv) != 0)
        return false;

    struct ui_panel_player_params *params = p;
    if (params) {
        const char *args[] = {"loadfile", params->path, NULL};
        mpv_command(priv->mpv, args);
    }
    return true;
}

static void player_uninit(struct ui_context *ctx)
{
    struct priv_panel *priv = ctx->priv_panel;
    if (priv->vo_uninit_fn)
        priv->vo_uninit_fn(ctx);
    if (priv->mpv)
        mpv_destroy(priv->mpv);
}

static void player_on_draw(struct ui_context *ctx)
{
    struct priv_panel *priv = ctx->priv_panel;
    if (priv->vo_draw_fn)
        priv->vo_draw_fn(ctx);
}

static void player_on_poll(struct ui_context *ctx)
{
    struct priv_panel *priv = ctx->priv_panel;
    if (!priv->mpv)
        return;

    while (true) {
        mpv_event *event = mpv_wait_event(priv->mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
            mpv_terminate_destroy(priv->mpv);
            priv->mpv = NULL;
            ui_panel_common_pop(ctx);
            break;
        }
    }
}

const struct ui_panel ui_panel_player = {
    .priv_size = sizeof(struct priv_panel),
    .init = player_init,
    .uninit = player_uninit,
    .on_show = NULL,
    .on_hide = NULL,
    .on_draw = player_on_draw,
    .on_poll = player_on_poll,
};
