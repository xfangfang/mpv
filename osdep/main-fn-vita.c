#include <pthread.h>

#include "libmpv/client.h"
#include "common/common.h"
#include "osdep/timer.h"
#include "osdep/vita/ui.h"
#include "ta/ta_talloc.h"

#define FRAME_INTERVAL_US (1 * 1000 * 1000 / 60)

#define INTERNAL_FLAG_REDRAW (1)
#define INTERNAL_FLAG_MPV_SHUTDOWN (1 << 1)

struct ui_context_internal {
    int flags;
    int64_t frame_start;
    pthread_mutex_t lock;
    pthread_cond_t wakeup;
    mpv_handle *mpv;
};

static void wait_next_frame(struct ui_context *ctx)
{
    int64_t frame_next = ctx->internal->frame_start + FRAME_INTERVAL_US;
    int64_t wait_time = MPMAX(frame_next - mp_time_us(), 0);
    if (wait_time > 0) {
        struct timespec ts = mp_time_us_to_timespec(wait_time);
        pthread_mutex_lock(&ctx->internal->lock);
        pthread_cond_timedwait(&ctx->internal->wakeup, &ctx->internal->lock, &ts);
        pthread_mutex_unlock(&ctx->internal->lock);
    }
}

static bool advnace_frame_time(struct ui_context *ctx)
{
    int frame_count = (mp_time_us() - ctx->internal->frame_start) / FRAME_INTERVAL_US;
    if (frame_count > 0) {
        ctx->internal->frame_start += frame_count * FRAME_INTERVAL_US;
        return true;
    }
    return false;
}

static void handle_redraw(struct ui_context *ctx)
{
    if (!(ctx->internal->flags & INTERNAL_FLAG_REDRAW))
        return;

    ctx->internal->flags &= ~INTERNAL_FLAG_REDRAW;
    ui_render_driver_vita.render_start(ctx);
    for (int i = 0; i < ctx->video_texture_count; ++i) {
        ui_render_driver_vita.texture_draw(ctx, ctx->video_textures[i], 0, 0, 1, 1);
    }
    ui_render_driver_vita.render_end(ctx);
}

static void handle_mpv_events(struct ui_context *ctx)
{
    mpv_handle *mpv = ctx->internal->mpv;
    if (!mpv)
        return;

    while (true) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
            mpv_terminate_destroy(mpv);
            ctx->internal->mpv = NULL;
            ctx->internal->flags |= INTERNAL_FLAG_MPV_SHUTDOWN;
            break;
        }
    }
}

static void on_dispatch_wakeup(void *p)
{
    struct ui_context *ctx = p;
    pthread_mutex_lock(&ctx->internal->lock);
    pthread_cond_signal(&ctx->internal->wakeup);
    pthread_mutex_unlock(&ctx->internal->lock);
}

static void ui_context_destroy(void *p)
{
    struct ui_context *ctx = p;
    for (int i = 0; i < ctx->video_texture_count; ++i) {
        struct ui_texture **p_texture = &ctx->video_textures[i];
        if (*p_texture)
            ui_render_driver_vita.texture_uninit(ctx, p_texture);
    }
    ctx->video_texture_count = 0;
    pthread_mutex_destroy(&ctx->internal->lock);
    pthread_cond_destroy(&ctx->internal->wakeup);
    if (ctx->priv_render)
        ui_render_driver_vita.uninit(ctx);
    if (ctx->priv_platform)
        ui_platform_driver_vita.uninit(ctx);
}

static struct ui_context *ui_context_new()
{
    size_t size_base = sizeof(struct ui_context);
    size_t size_internal = sizeof(struct ui_context_internal);
    size_t size_platform = ui_platform_driver_vita.priv_size;
    size_t size_render = ui_render_driver_vita.priv_size;
    size_t size_all = size_base + size_internal + size_platform + size_render;

    uint8_t *p = talloc_zero_size(NULL, size_all);
    struct ui_context *ctx = (void*) p;
    talloc_set_destructor(ctx, ui_context_destroy);
    ctx->dispatch = mp_dispatch_create(ctx);
    mp_dispatch_set_wakeup_fn(ctx->dispatch, on_dispatch_wakeup, ctx);
    p += size_base;

    ctx->internal = (void*) p;
    pthread_mutex_init(&ctx->internal->lock, NULL);
    pthread_cond_init(&ctx->internal->wakeup, NULL);
    p += size_internal;

    ctx->priv_platform = (void*) p;
    if (!ui_platform_driver_vita.init(ctx)) {
        ctx->priv_platform = NULL;
        goto error;
    }
    p += size_platform;

    ctx->priv_render = (void*) p;
    if (!ui_render_driver_vita.init(ctx)) {
        ctx->priv_render = NULL;
        goto error;
    }

    ctx->internal->mpv = mpv_create();
    if (!ctx->internal->mpv)
        goto error;

    mpv_set_option(ctx->internal->mpv, "wid", MPV_FORMAT_INT64, &ctx);
    mpv_set_option_string(ctx->internal->mpv, "idle", "yes");
    mpv_set_wakeup_callback(ctx->internal->mpv, on_dispatch_wakeup, ctx);
    if (mpv_initialize(ctx->internal->mpv) != 0)
        goto error;

    return ctx;

error:
    talloc_free(ctx);
    return NULL;
}

static void main_loop(struct ui_context *ctx)
{
    if (!ctx)
        return;

    const char *args[] = {"loadfile", "/tmp/test.mp4", NULL};
    mpv_command(ctx->internal->mpv, args);

    while (true) {
        // poll and run pending async jobs
        handle_mpv_events(ctx);
        mp_dispatch_queue_process(ctx->dispatch, 0);

        if (advnace_frame_time(ctx))
            handle_redraw(ctx);

        if (ctx->internal->flags & INTERNAL_FLAG_MPV_SHUTDOWN)
            break;

        // sleep until next frame or interrupt to avoid CPU stress
        wait_next_frame(ctx);
    }
}

int main(int argc, char *argv[])
{
    struct ui_context *ctx = ui_context_new();
    main_loop(ctx);
    talloc_free(ctx);
    return 0;
}

void ui_request_redraw(struct ui_context *ctx)
{
    ctx->internal->flags |= INTERNAL_FLAG_REDRAW;
}

void ui_request_mpv_shutdown(struct ui_context *ctx)
{
    const char *args[] = {"quit", NULL};
    mpv_command_async(ctx->internal->mpv, 0, args);
}
