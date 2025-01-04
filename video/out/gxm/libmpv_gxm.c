#include "common/msg.h"
#include "video/out/gpu/context.h"
#include "video/out/gpu/libmpv_gpu.h"
#include "video/out/gxm/ra_gxm.h"
#include "libmpv/render_gxm.h"

struct priv {
    SceGxmContext *context;
    SceGxmShaderPatcher *shader_patcher;
    struct ra_ctx *ra_ctx;
    struct ra_tex *tex;
};

static int init(struct libmpv_gpu_context *ctx, mpv_render_param *params)
{
    struct priv *p = ctx->priv = talloc_zero(NULL, struct priv);

    mpv_gxm_init_params *init_params =
            get_mpv_render_param(params, MPV_RENDER_PARAM_GXM_INIT_PARAMS, NULL);
    if (!init_params)
        return MPV_ERROR_INVALID_PARAMETER;

    p->context = init_params->context;
    p->shader_patcher = init_params->shader_patcher;

    // initialize a blank ra_ctx to reuse ra_gl_ctx
    p->ra_ctx = talloc_zero(p, struct ra_ctx);
    p->ra_ctx->log = ctx->log;
    p->ra_ctx->global = ctx->global;

    p->ra_ctx->ra = ra_gxm_create(p->context, p->shader_patcher, ctx->log, init_params->buffer_index);
    if (!p->ra_ctx->ra)
        return MPV_ERROR_UNSUPPORTED;

    p->tex = talloc_zero(p, struct ra_tex);

    ctx->ra_ctx = p->ra_ctx;
    return 0;
}

static int wrap_fbo(struct libmpv_gpu_context *ctx, mpv_render_param *params, struct ra_tex **out)
{
    struct priv *p = ctx->priv;
    struct ra *ra = p->ra_ctx->ra;
    struct ra_format *fmt = NULL;

    mpv_gxm_fbo *fbo = get_mpv_render_param(params, MPV_RENDER_PARAM_GXM_FBO, NULL);
    if (!fbo)
        return MPV_ERROR_INVALID_PARAMETER;

    for (int n = 0; n < ra->num_formats; n++) {
        const struct gxm_format *gf = ra->formats[n]->priv;
        if (gf->format == fbo->format) {
            fmt = ra->formats[n];
            break;
        }
    }

    p->tex->params = (struct ra_tex_params) {
            .dimensions = 2,
            .w          = fbo->w,
            .h          = fbo->h,
            .d          = 1,
            .format     = fmt,
            .render_dst = true,
            // TODO: blit_src and blit_dst
    };
    p->tex->priv = fbo->tex;

    *out = p->tex;
    return 0;
}

static void start_frame(struct libmpv_gpu_context *ctx)
{
    struct priv *p = ctx->priv;
    NVGXMframebuffer *fbo = p->tex->priv;
    if (!fbo)
        return;
    gxmBeginFrameEx(fbo, 0);
}

static void done_frame(struct libmpv_gpu_context *ctx, bool ds)
{
    struct priv *p = ctx->priv;
    NVGXMframebuffer *fbo = p->tex->priv;
    if (!fbo)
        return;
    gxmEndFrame();
}

static void destroy(struct libmpv_gpu_context *ctx)
{
    struct ra_ctx *ra_ctx = ctx->ra_ctx;
    if (ra_ctx->ra)
        ra_ctx->ra->fns->destroy(ra_ctx->ra);
}

const struct libmpv_gpu_context_fns libmpv_gpu_context_gxm = {
        .api_name    = MPV_RENDER_API_TYPE_GXM,
        .init        = init,
        .wrap_fbo    = wrap_fbo,
        .done_frame  = done_frame,
        .destroy     = destroy,
        .start_frame = start_frame,
};