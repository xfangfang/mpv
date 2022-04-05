#include <pthread.h>

#include "vo.h"
#include "osdep/vita/ui.h"
#include "video/mp_image.h"
#include "video/img_format.h"

struct tex_params {
    int w;
    int h;
    enum ui_tex_fmt fmt;
};

struct priv_draw {
    struct ui_texture *texture;
    struct ui_context *context;
    mp_image_t *image;
    pthread_mutex_t *lock;
};

struct priv_vo {
    pthread_mutex_t lock;
    struct priv_draw *draw_ctx;
    struct tex_params *tex_params;
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

static void do_run_update_texture(void *vo)
{
    struct ui_context *ctx = get_ui_context(vo);
    struct priv_draw *priv_draw = ctx->video_ctx;
    struct ui_texture *texture = priv_draw->texture;
    pthread_mutex_lock(priv_draw->lock);
    mp_image_t *image = priv_draw->image;
    if (image && texture) {
        void *planes[MP_MAX_PLANES];
        for (int i = 0; i < MP_MAX_PLANES; ++i)
            planes[i] = image->planes[i];
        ui_render_driver_vita.texture_upload(ctx, texture, planes, image->stride, image->num_planes);
    }
    pthread_mutex_unlock(priv_draw->lock);
}

static void do_run_init_texture(void *vo)
{
    struct vo *vo_cast = vo;
    struct priv_vo *priv_vo = vo_cast->priv;
    pthread_mutex_lock(&priv_vo->lock);
    if (priv_vo->tex_params) {
        struct tex_params *params = priv_vo->tex_params;
        struct ui_context *ctx = get_ui_context(vo);
        struct ui_texture **p_tex = &priv_vo->draw_ctx->texture;
        if (*p_tex) {
            ui_render_driver_vita.texture_uninit(ctx, p_tex);
        }
        ui_render_driver_vita.texture_init(ctx, p_tex, params->fmt, params->w, params->h);
        TA_FREEP(&priv_vo->tex_params);
    }
    pthread_mutex_unlock(&priv_vo->lock);
}

static enum ui_tex_fmt resolve_tex_fmt(int fmt)
{
    switch (fmt) {
    case IMGFMT_RGBA:
        return TEX_FMT_RGBA;
    default:
        return TEX_FMT_UNKNOWN;
    }
}

static int query_format(struct vo *vo, int format)
{
    return resolve_tex_fmt(format) != TEX_FMT_UNKNOWN;
}

static void do_video_draw(void *video_ctx)
{
    struct priv_draw *priv = video_ctx;
    struct ui_context *ctx = priv->context;
    pthread_mutex_lock(priv->lock);
    if (priv->texture)
        ui_render_driver_vita.texture_draw(ctx, priv->texture, 0, 0, 1, 1);
    pthread_mutex_unlock(priv->lock);
}

static void do_video_uninit(void *video_ctx)
{
    struct priv_draw *priv = video_ctx;
    struct ui_context *ctx = priv->context;
    if (priv->texture)
        ui_render_driver_vita.texture_uninit(ctx, &priv->texture);
    TA_FREEP(&priv->image);

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
    struct ui_context *ctx = get_ui_context(vo);
    struct priv_vo *priv_vo = vo->priv;
    struct priv_draw *priv_draw = talloc_zero_size(ctx, sizeof(struct priv_draw));
    pthread_mutex_init(&priv_vo->lock, NULL);
    priv_vo->draw_ctx = priv_draw;
    priv_draw->context = ctx;
    priv_draw->lock = &priv_vo->lock;

    // ui_context field assignments should be run on main thread
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
    cancel_main_thread_cb(vo, do_run_init_texture);
    cancel_main_thread_cb(vo, do_run_update_texture);
}

static void flip_page(struct vo *vo)
{
    post_main_thread_cb(vo, do_run_request_redraw);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    // reinit video texture
    struct priv_vo *priv_vo = vo->priv;
    pthread_mutex_lock(&priv_vo->lock);
    TA_FREEP(&priv_vo->tex_params);
    priv_vo->tex_params = talloc_zero_size(priv_vo, sizeof(struct tex_params));
    *priv_vo->tex_params = (struct tex_params) {
        .w = params->w,
        .h = params->h,
        .fmt = resolve_tex_fmt(params->imgfmt),
    };
    pthread_mutex_unlock(&priv_vo->lock);
    post_main_thread_cb(vo, do_run_init_texture);


    //TODO adjust video size and position
    return 0;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    // swap current image ref
    struct priv_vo *priv = vo->priv;
    pthread_mutex_lock(&priv->lock);
    TA_FREEP(&priv->draw_ctx->image);
    if (frame->current)
        priv->draw_ctx->image = mp_image_new_ref(frame->current);
    pthread_mutex_unlock(&priv->lock);

    // update video texutre
    post_main_thread_cb(vo, do_run_update_texture);
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
