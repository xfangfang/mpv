#include "vo.h"
#include "osdep/vita/ui.h"
#include "video/mp_image.h"

static struct ui_context *get_ui_context(struct vo *vo)
{
    return (struct ui_context*) vo->opts->WinID;
}

static void do_request_redraw(void *vo)
{
    ui_request_redraw(get_ui_context(vo));
}

static int query_format(struct vo *vo, int format)
{
    return ui_render_driver_vita.texture_is_supported(format);
}

static int preinit(struct vo *vo)
{
    return 0;
}

static void uninit(struct vo *vo)
{
    struct ui_context *ctx = get_ui_context(vo);
    mp_dispatch_cancel_fn(ctx->dispatch, do_request_redraw, vo);
}

static void flip_page(struct vo *vo)
{
    struct ui_context *ctx = get_ui_context(vo);
    mp_dispatch_enqueue_notify(ctx->dispatch, do_request_redraw, vo);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    //TODO adjust video size and position
    return 0;
}

static void update_video_textures(void *p)
{
    void **pp = p;
    struct ui_context *ctx = pp[1];
    struct vo_frame *frame = pp[2];
    struct mp_image *image = frame->current;
    ctx->video_texture_count = image ? image->num_planes : 0;
    for (int i = 0; i < ctx->video_texture_count; ++i) {
        struct ui_texture **tex = &ctx->video_textures[i];
        if (!*tex) {
            int w = image->w;
            int h = image->h;
            ui_render_driver_vita.texture_init(ctx, tex, image->imgfmt, w, h);
        }
        ui_render_driver_vita.texture_upload(ctx, *tex, image->planes[i], image->stride[i]);
    }
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct ui_context *ctx = get_ui_context(vo);
    void *p[] = {vo, ctx, frame};
    mp_dispatch_run(ctx->dispatch, update_video_textures, p);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_vita = {
    .description = "Vita video output",
    .name = "Vita",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
};
