#include <psp2/gxm.h>

#include "common/msg.h"
#include "osdep/io.h"
#include "osdep/subprocess.h"
#include "osdep/timer.h"
#include "video/out/gpu/utils.h"
#include "video/out/gxm/ra_gxm.h"

#include "osdep/vita/include/nanovg_gxm_utils.h"
#include "nanovg_gxm_utils.h"


const struct gxm_format gxm_formats[] = {
        {"r8",      1, 1, {8},              SCE_GXM_TEXTURE_FORMAT_U8_000R,              RA_CTYPE_UNORM, true, true, true,  true},
        {"rg8",     2, 2, {8,  8},          SCE_GXM_TEXTURE_FORMAT_U8U8_GRGR,           RA_CTYPE_UNORM, true, true, true,  true},
        {"rgba8",   4, 4, {8,  8,  8,  8},  SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA,     RA_CTYPE_UNORM, true, true, true,  true},
        {"r16",     1, 2, {16},             SCE_GXM_TEXTURE_FORMAT_U16_000R,             RA_CTYPE_UNORM, true, true, true,  true},
        {"rg16",    2, 4, {16, 16},         SCE_GXM_TEXTURE_FORMAT_U16U16_GRGR,         RA_CTYPE_UNORM, true, true, true,  true},
        {"rgba16",  4, 8, {16, 16, 16, 16}, SCE_GXM_TEXTURE_FORMAT_U16U16U16U16_RGBA, RA_CTYPE_UNORM, true, true, true,  true},

        {"r32ui",    1, 4,  {32},             SCE_GXM_TEXTURE_FORMAT_S32_000R,               RA_CTYPE_UINT,  true,  false, true,  true},
//        {"rg32ui",   2, 8,  {32, 32},         DkImageFormat_RG32_Uint,              RA_CTYPE_UINT,  true,  false, true,  true},
//        {"rgb32ui",  3, 12, {32, 32, 32},     DkImageFormat_RGB32_Uint,             RA_CTYPE_UINT,  false, false, false, true},
//        {"rgba32ui", 4, 16, {32, 32, 32, 32}, DkImageFormat_RGBA32_Uint,            RA_CTYPE_UINT,  true,  false, true,  true},
//
        {"r16f",    1, 2, {16},             SCE_GXM_TEXTURE_FORMAT_F16_000R,             RA_CTYPE_FLOAT, true, true, true,  true},
        {"rg16f",   2, 4, {16, 16},         SCE_GXM_TEXTURE_FORMAT_F16F16_GRGR,         RA_CTYPE_FLOAT, true, true, true,  true},
        {"rgba16f", 4, 8, {16, 16, 16, 16}, SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_RGBA, RA_CTYPE_FLOAT, true, true, true,  true},
        {"r32f",    1, 4, {32},             SCE_GXM_TEXTURE_FORMAT_F32_000R,             RA_CTYPE_FLOAT, true, true, true,  true},
        {"rg32f",   2, 8, {32, 32},         SCE_GXM_TEXTURE_FORMAT_F32F32_GRGR,         RA_CTYPE_FLOAT, true, true, true,  true},
//        {"rgb32f",   3, 12, {32, 32, 32},     SCE_GXM_TEXTURE_FORMAT_F32_R,            RA_CTYPE_FLOAT, false, false, false, true},
//        {"rgba32f",  4, 16, {32, 32, 32, 32}, DkImageFormat_RGBA32_Float,           RA_CTYPE_FLOAT, true,  true,  true,  true},
//
//        {"rgb10_a2", 4, 4,  {10, 10, 10, 2},  DkImageFormat_RGB10A2_Unorm,          RA_CTYPE_UNORM, true,  true,  true,  true},
//        {"rg11b10f", 3, 4,  {11, 11, 10},     DkImageFormat_RG11B10_Float,          RA_CTYPE_FLOAT, true,  true,  true,  true},
        {"bgra8",   4, 4, {8,  8,  8,  8},  SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA,     RA_CTYPE_UNORM, true, true, true,  false},
        {"bgrx8",   3, 4, {8,  8,  8},      SCE_GXM_TEXTURE_FORMAT_U8U8U8X8_BGR1,     RA_CTYPE_UNORM, true, true, false, false},
};


struct gxm_vao {
    SceUID uid;
    float *buffer;
    int stride;     // size of each element (interleaved elements are assumed)
    const struct ra_renderpass_input *entries;
    int num_entries;
};

struct ra_renderpass_gxm {
    NVGXMshaderProgram prog;
    SceUID vertices_uid;
    float *vertices;
    const SceGxmProgramParameter **uniform_loc;
    int num_uniform_loc;

    float *uniform_buffer;
    SceUID uniform_uid;
};


static void gxm_tex_destroy(struct ra *ra, struct ra_tex *tex) {
    if (!tex)
        return;
    talloc_free(tex);
}

static struct ra_tex *gxm_tex_create(struct ra *ra,
                                 const struct ra_tex_params *params) {
    if (params->downloadable)
        return NULL;

    int ret;
    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    if (!tex) {
        goto error;
    }

    assert(params->format != NULL);
    const struct gxm_format *fmt = params->format->priv;
    int aligned_width = ALIGN(params->w, SCE_GXM_TEXTURE_ALIGNMENT);
    int tex_size = aligned_width * params->h * fmt->bytes;
    const unsigned char * data = params->initial_data;
    tex->params = *params;
    tex->params.initial_data = NULL;

    struct ra_tex_gxm *tex_gxm = tex->priv = talloc_zero(tex, struct ra_tex_gxm);
    if (!tex_gxm) {
        goto error;
    }

    tex_gxm->tex_data = (uint8_t *) gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                                  SCE_GXM_MEMORY_ATTRIB_RW,
                                                  tex_size,
                                                  &tex_gxm->data_UID);
    if (tex_gxm->tex_data == NULL) {
        goto error;
    }

    if (data == NULL) {
        memset(tex_gxm->tex_data, 0, tex_size);
    } else {
        for (int i = 0; i < params->h; i++) {
            memcpy(tex_gxm->tex_data + i * aligned_width, data + i * params->w * fmt->bytes, params->w * fmt->bytes);
        }
    }

    ret = sceGxmTextureInitLinear(&tex_gxm->gxm_tex, tex_gxm->tex_data, fmt->format,
                                  aligned_width, params->h, 0);
    if (ret < 0) {
        gpu_unmap_free(tex_gxm->data_UID);
        tex_gxm->data_UID = 0;
        goto error;
    }

    SceGxmTextureFilter filter = params->src_linear ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    sceGxmTextureSetMinFilter(&tex_gxm->gxm_tex, filter);
    sceGxmTextureSetMagFilter(&tex_gxm->gxm_tex, filter);

    SceGxmTextureAddrMode wrap = params->src_repeat ? SCE_GXM_TEXTURE_ADDR_REPEAT : SCE_GXM_TEXTURE_ADDR_CLAMP;
    sceGxmTextureSetUAddrMode(&tex_gxm->gxm_tex, wrap);
    sceGxmTextureSetVAddrMode(&tex_gxm->gxm_tex, wrap);

    return tex;
    error:
    gxm_tex_destroy(ra, tex);
    return NULL;
}


static bool gxm_tex_upload(struct ra *ra, const struct ra_tex_upload_params *params) {
    struct ra_tex *tex = params->tex;
    struct ra_buf *buf = params->buf;
    struct ra_tex_gxm *tex_gxm = tex->priv;
    assert(tex->params.host_mutable);
    assert(!params->buf || !params->src);

    // todo: 是不是可以删除这里的buf
    const void *src = params->src;
    if (buf) {
        src = (void *) params->buf_offset;
    }

    switch (tex->params.dimensions) {
        case 1:
            //todo
            break;
        case 2: {
            struct mp_rect rc = {0, 0, tex->params.w, tex->params.h};
            if (params->rc)
                rc = *params->rc;
            // todo: 根据实际像素格式，设置 bpp
            // todo: psv 纹理 stride 可能和内置 stride 不同
            int bpp = 1;
            for (int i = 0; i < rc.y1 - rc.y0; i++) {
                int start = (i + rc.y0) * params->stride + rc.x0 * bpp;
                memcpy(tex_gxm->tex_data + start, src + start, (rc.x1 - rc.x0) * bpp);
            }
            break;
        }
        case 3:
            //todo
            break;
    }

    return true;
}

static bool gxm_tex_download(struct ra *ra, struct ra_tex_download_params *params) {
    return false;
}

static void gxm_buf_destroy(struct ra *ra, struct ra_buf *buf) {
    if (!buf)
        return;
    struct d3d_buf *buf_p = buf->priv;

    talloc_free(buf);
}

static struct ra_buf *gxm_buf_create(struct ra *ra,
                                 const struct ra_buf_params *params) {
    return NULL;
}


static void gxm_buf_update(struct ra *ra, struct ra_buf *buf, ptrdiff_t offset,
                       const void *data, size_t size) {
    struct d3d_buf *buf_p = buf->priv;
}


static void gxm_clear(struct ra *ra, struct ra_tex *tex, float color[4],
                  struct mp_rect *rc) {
    gxmScissor(rc->x0, rc->y0, rc->x1 - rc->x0, rc->y1 - rc->y0);
    gxmClearColor(color[0], color[1], color[2], color[3]);
    gxmClear();
}


static void gxm_blit(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                 struct mp_rect *dst_rc_ptr, struct mp_rect *src_rc_ptr) {
    struct ra_d3d11 *p = ra->priv;
    struct d3d_tex *dst_p = dst->priv;
    struct d3d_tex *src_p = src->priv;
    struct mp_rect dst_rc = *dst_rc_ptr;
    struct mp_rect src_rc = *src_rc_ptr;

    assert(src->params.blit_src);
    assert(dst->params.blit_dst);

}

static int gxm_desc_namespace(struct ra *ra, enum ra_vartype type) {
    return type;
}

static void gxm_renderpass_destroy(struct ra *ra, struct ra_renderpass *pass) {
    if (!pass)
        return;
    struct ra_renderpass_gxm *pass_p = pass->priv;
    struct ra_gxm *gxm = ra->priv;

    sceGxmFinish(gxm->context);
    gpu_unmap_free(pass_p->vertices_uid);
    gpu_unmap_free(pass_p->uniform_uid);
    gxmDeleteShader(&pass_p->prog);
    talloc_free(pass);
}

static struct ra_renderpass *gxm_renderpass_create(struct ra *ra,
                                               const struct ra_renderpass_params *params) {
    struct ra_gxm *gxm = ra->priv;
    struct ra_renderpass *pass = talloc_zero(NULL, struct ra_renderpass);
    pass->params = *ra_renderpass_params_copy(pass, params);
    pass->params.cached_program = (bstr) {0};
    struct ra_renderpass_gxm *p = pass->priv = talloc_zero(pass, struct ra_renderpass_gxm);

//    sceClibPrintf("frag: %s\n\n", params->frag_shader);
//    sceClibPrintf("vert: %s\n\n", params->vertex_shader);
//    sceClibPrintf("comp: %p\n\n", params->compute_shader);

    if (params->type == RA_RENDERPASS_TYPE_COMPUTE) {
        MP_ASSERT_UNREACHABLE();
    }

    gxmCreateShader(&p->prog, "mpv", (const char *) params->vertex_shader, (const char *) params->frag_shader);
    if (p->prog.vert_gxp == NULL || p->prog.frag_gxp == NULL) {
        talloc_free(pass);
        return NULL;
    }

    SceGxmVertexAttribute *clear_vertex_attribute = talloc_array(NULL, SceGxmVertexAttribute,
                                                                 pass->params.num_vertex_attribs);
    for (int i = 0; i < pass->params.num_vertex_attribs; i++) {
        struct ra_renderpass_input *inp = &pass->params.vertex_attribs[i];
        const SceGxmProgramParameter *param = sceGxmProgramFindParameterByName(p->prog.vert_gxp, inp->name);

        clear_vertex_attribute[i].streamIndex = 0;
        clear_vertex_attribute[i].offset = inp->offset;
        clear_vertex_attribute[i].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        clear_vertex_attribute[i].componentCount = inp->dim_m * inp->dim_v;
        clear_vertex_attribute[i].regIndex = sceGxmProgramParameterGetResourceIndex(param);
    }

    p->vertices = (float *) gpu_alloc_map(
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            SCE_GXM_MEMORY_ATTRIB_READ,
            pass->params.vertex_stride * 6,
            &p->vertices_uid);

    SceGxmVertexStream clear_vertex_stream[1];
    clear_vertex_stream[0].stride = pass->params.vertex_stride;
    clear_vertex_stream[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

    sceGxmShaderPatcherCreateVertexProgram(
            gxm->shader_patcher, p->prog.vert_id,
            clear_vertex_attribute, pass->params.num_vertex_attribs,
            clear_vertex_stream, sizeof(clear_vertex_stream) / sizeof(SceGxmVertexStream),
            &p->prog.vert);

    sceGxmShaderPatcherCreateFragmentProgram(
            gxm->shader_patcher, p->prog.frag_id,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE,
            NULL, p->prog.vert_gxp,
            &p->prog.frag);

    talloc_free(clear_vertex_attribute);

    int uniform_size = 0;
    for (int n = 0; n < pass->params.num_inputs; n++) {
        const SceGxmProgramParameter *loc = sceGxmProgramFindParameterByName(p->prog.frag_gxp, params->inputs[n].name);
        MP_TARRAY_APPEND(p, p->uniform_loc, p->num_uniform_loc, loc);
        if (pass->params.inputs[n].type == RA_VARTYPE_INT || pass->params.inputs[n].type == RA_VARTYPE_FLOAT) {
            uniform_size += ALIGN(pass->params.inputs[n].dim_m * pass->params.inputs[n].dim_v, 4);
        }
    }

    p->uniform_buffer = (float *) gpu_alloc_map(
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            SCE_GXM_MEMORY_ATTRIB_READ,
            sizeof(float) * uniform_size,
            &p->uniform_uid);
    sceGxmSetFragmentUniformBuffer(gxm->context, gxm->buffer_index, p->uniform_buffer);

    return pass;
}

static void gxm_renderpass_run(struct ra *ra,
                           const struct ra_renderpass_run_params *params) {
    struct ra_gxm *gxm = ra->priv;
    struct ra_renderpass *pass = params->pass;
    enum ra_renderpass_type type = pass->params.type;
    struct ra_renderpass_gxm *p = pass->priv;

    assert(type != RA_RENDERPASS_TYPE_COMPUTE);

    sceGxmSetVertexProgram(gxm->context, p->prog.vert);
    sceGxmSetFragmentProgram(gxm->context, p->prog.frag);
    memcpy(p->vertices, params->vertex_data, pass->params.vertex_stride * params->vertex_count);
    sceGxmSetVertexStream(gxm->context, 0, p->vertices);

    for (int n = 0; n < params->num_values; n++) {
        struct ra_renderpass_input_val *val = &params->values[n];
        struct ra_renderpass_input *input = &pass->params.inputs[val->index];
        const SceGxmProgramParameter *loc = p->uniform_loc[val->index];

        switch (input->type) {
            case RA_VARTYPE_INT:
            //todo 检查int值的作用
            case RA_VARTYPE_FLOAT: {
                if (!loc)
                    break;
                float *f = val->data;
                sceGxmSetUniformDataF(p->uniform_buffer, loc, 0, input->dim_v * input->dim_m, f);
                break;
            }
            case RA_VARTYPE_TEX: {
                struct ra_tex *tex = *(struct ra_tex **) val->data;
                struct ra_tex_gxm *tex_gxm = tex->priv;
                assert(tex->params.render_src);
                sceGxmSetFragmentTexture(gxm->context, input->binding, &tex_gxm->gxm_tex);
                break;
            }
            case RA_VARTYPE_IMG_W:
            case RA_VARTYPE_BUF_RO:
            case RA_VARTYPE_BUF_RW:
            default:
                MP_ASSERT_UNREACHABLE();
        }
    }

    sceGxmDraw(gxm->context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, gxmGetSharedIndices(),
               params->vertex_count);
}

static void gxm_timer_destroy(struct ra *ra, ra_timer *ratimer) {
    if (!ratimer)
        return;
    struct gxm_timer *timer = ratimer;
    talloc_free(timer);
}

static ra_timer *gxm_timer_create(struct ra *ra) {
    return NULL;
}

static void gxm_timer_start(struct ra *ra, ra_timer *ratimer) {
}

static uint64_t gxm_timer_stop(struct ra *ra, ra_timer *ratimer) {
    return 0;
}


static void gxm_debug_marker(struct ra *ra, const char *msg) {
}

static void gxm_destroy(struct ra *ra) {
    talloc_free(ra);
}

static struct ra_fns ra_fns_gxm = {
        .destroy            = gxm_destroy,
        .tex_create         = gxm_tex_create,
        .tex_destroy        = gxm_tex_destroy,
        .tex_upload         = gxm_tex_upload,
        .tex_download       = gxm_tex_download,
        .buf_create         = gxm_buf_create,
        .buf_destroy        = gxm_buf_destroy,
        .buf_update         = gxm_buf_update,
        .clear              = gxm_clear,
        .blit               = gxm_blit,
        .uniform_layout     = std140_layout,
        .desc_namespace     = gxm_desc_namespace,
        .renderpass_create  = gxm_renderpass_create,
        .renderpass_destroy = gxm_renderpass_destroy,
        .renderpass_run     = gxm_renderpass_run,
        .timer_create       = gxm_timer_create,
        .timer_destroy      = gxm_timer_destroy,
        .timer_start        = gxm_timer_start,
        .timer_stop         = gxm_timer_stop,
        .debug_marker       = gxm_debug_marker,
};


struct ra *
ra_gxm_create(SceGxmContext *context, SceGxmShaderPatcher *shader_patcher, struct mp_log *log, int buffer_index) {
    struct ra *ra = talloc_zero(NULL, struct ra);
    ra->log = log;
    ra->fns = &ra_fns_gxm;

    ra->glsl_version = 200;
    ra->glsl_gxm = true;
    ra->gxm_buffer_index = buffer_index;

    ra->caps = RA_CAP_DIRECT_UPLOAD | RA_CAP_GLOBAL_UNIFORM;

    struct ra_gxm *p = ra->priv = talloc_zero(ra, struct ra_gxm);
    p->context = context;
    p->shader_patcher = shader_patcher;
    p->buffer_index = buffer_index;

    ra->max_texture_wh = 1920;

    for (int i = 0; i < MP_ARRAY_SIZE(gxm_formats); ++i) {
        const struct gxm_format *gxmfmt = &gxm_formats[i];

        struct ra_format *fmt = talloc_zero(ra, struct ra_format);
        *fmt = (struct ra_format) {
                .name           = gxmfmt->name,
                .priv           = (void *) gxmfmt,
                .ctype          = gxmfmt->ctype,
                .ordered        = gxmfmt->ordered,
                .num_components = gxmfmt->components,
                .pixel_size     = gxmfmt->bytes,
                .linear_filter  = gxmfmt->linear_filter,
                .renderable     = gxmfmt->renderable,
                .storable       = gxmfmt->storable,
        };

        for (int j = 0; j < gxmfmt->components; j++)
            fmt->component_size[j] = fmt->component_depth[j] = gxmfmt->bits[j];

        fmt->glsl_format = ra_fmt_glsl_format(fmt);

        MP_TARRAY_APPEND(ra, ra->formats, ra->num_formats, fmt);
    }
    return ra;
}


bool ra_is_gxm(struct ra *ra) {
    return ra->fns == &ra_fns_gxm;
}
