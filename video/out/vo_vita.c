#include <pthread.h>

#include "vo.h"
#include "osdep/vita/ui.h"
#include "video/mp_image.h"
#include "video/img_format.h"

struct priv_draw {
    int count;
    struct ui_texture *textures[MP_MAX_PLANES];
    struct ui_context *context;
    struct {
        mp_image_t *image;
        pthread_mutex_t *lock;
    } shared_data;
};

struct priv_vo {
    struct priv_draw *draw_ctx;
    pthread_mutex_t lock;
};

static struct ui_context *get_ui_context(struct vo *vo)
{
    // this pointer is passed as option value before MPV initialization
    // it should be valid in vo_driver's whole lifetime
    return (struct ui_context*) vo->opts->WinID;
}

static void post_main_thread_cb(struct vo *vo, mp_dispatch_fn func)
{
    mp_dispatch_enqueue_notify(get_ui_context(vo)->dispatch, func, vo);
}

static void cancel_main_thread_cb(struct vo *vo, mp_dispatch_fn func)
{
    mp_dispatch_cancel_fn(get_ui_context(vo)->dispatch, func, vo);
}

static void do_run_request_redraw(void *vo)
{
    ui_request_redraw(get_ui_context(vo));
}

static void do_run_update_textures(void *vo)
{
    struct ui_context *ctx = get_ui_context(vo);
    struct priv_draw *priv_draw = ctx->video_ctx;
    pthread_mutex_lock(priv_draw->shared_data.lock);
    mp_image_t *image = priv_draw->shared_data.image;
    priv_draw->count = image ? image->num_planes : 0;
    for (int i = 0; i < priv_draw->count; ++i) {
        struct ui_texture **tex = &priv_draw->textures[i];
        if (!*tex) {
            int w = image->w;
            int h = image->h;
            ui_render_driver_vita.texture_init(ctx, tex, image->imgfmt, w, h);
        }
        ui_render_driver_vita.texture_upload(ctx, *tex, image->planes[i], image->stride[i]);
    }
    pthread_mutex_unlock(priv_draw->shared_data.lock);
}

static void do_video_draw(void *video_ctx)
{
    struct priv_draw *priv = video_ctx;
    struct ui_context *ctx = priv->context;
    pthread_mutex_lock(priv->shared_data.lock);
    for (int i = 0; i < priv->count; ++i)
        ui_render_driver_vita.texture_draw(ctx, priv->textures[i], 0, 0, 1, 1);
    pthread_mutex_unlock(priv->shared_data.lock);
}

static int query_format(struct vo *vo, int format)
{
    return ui_render_driver_vita.texture_is_supported(format);
}

static void do_video_uninit(void *video_ctx)
{
    struct priv_draw *priv = video_ctx;
    struct ui_context *ctx = priv->context;
    for (int i = 0; i < priv->count; ++i)
        ui_render_driver_vita.texture_uninit(ctx, &priv->textures[i]);
    talloc_free(priv->shared_data.image);
    priv->count = 0;
    priv->shared_data.image = NULL;

    ctx->video_ctx = NULL;
    ctx->video_draw_cb = NULL;
    ctx->video_uninit_cb = NULL;
}

static void do_run_init_draw_ctx(void *vo)
{
    struct vo *vo_cast = vo;
    struct priv_vo *priv = vo_cast->priv;
    struct ui_context *ctx = get_ui_context(vo);
    ctx->video_ctx = priv->draw_ctx;
    ctx->video_draw_cb = do_video_draw;
    ctx->video_uninit_cb = do_video_uninit;
}

static int preinit(struct vo *vo)
{
    // ui_context modification should be run on main thread
    struct ui_context *ctx = get_ui_context(vo);
    struct priv_vo *priv_vo = vo->priv;
    struct priv_draw *priv_draw = talloc_zero_size(ctx, sizeof(struct priv_draw));
    pthread_mutex_init(&priv_vo->lock, NULL);
    priv_vo->draw_ctx = priv_draw;
    priv_draw->context = ctx;
    priv_draw->shared_data.lock = &priv_vo->lock;
    post_main_thread_cb(vo, do_run_init_draw_ctx);
    return 0;
}

static void uninit(struct vo *vo)
{
    // main thread is waiting for MPV uninit now,
    // it should be safe to free mutex here
    struct priv_vo *priv = vo->priv;
    pthread_mutex_destroy(&priv->lock);
    cancel_main_thread_cb(vo, do_run_init_draw_ctx);
    cancel_main_thread_cb(vo, do_run_request_redraw);
    cancel_main_thread_cb(vo, do_run_update_textures);
}

static void flip_page(struct vo *vo)
{
    post_main_thread_cb(vo, do_run_request_redraw);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    //TODO adjust video size and position
    return 0;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv_vo *priv = vo->priv;
    pthread_mutex_lock(&priv->lock);
    mp_image_t **p_image = &priv->draw_ctx->shared_data.image;
    if (*p_image) {
        talloc_free(*p_image);
        *p_image = NULL;
    }
    if (frame->current)
        *p_image = mp_image_new_ref(frame->current);
    pthread_mutex_unlock(&priv->lock);

    post_main_thread_cb(vo, do_run_update_textures);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_vita = {
    .description = "Vita video output",
    .priv_size = sizeof(struct priv_vo),
    .name = "Vita",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
};
