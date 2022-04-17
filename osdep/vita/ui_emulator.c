#include "ui.h"
#include "video/img_format.h"
#include "audio/out/internal.h"

#include <GLFW/glfw3.h>

#define TEXTURE_BUFFER_SIZE     (1024 * 1024 * 4)

extern const struct ao_driver audio_out_alsa;
struct ao_driver audio_out_vita;

struct gl_attr_spec {
    const char *name;
    int pos;
};

static const char *const shader_vert_source =
    "attribute vec4 a_draw_pos;"
    "attribute vec2 a_texture_pos;"
    "varying vec2 v_texture_pos;"
    "void main() {"
    "    gl_Position = a_draw_pos;"
    "    v_texture_pos = a_texture_pos;"
    "}";

static const struct gl_attr_spec attr_draw_tex_pos_draw = { .name = "a_draw_pos", .pos = 0, };
static const struct gl_attr_spec attr_draw_tex_pos_tex = { .name = "a_texture_pos", .pos = 1, };

struct gl_tex_plane_spec {
    int bpp;
    int div;
    GLenum fmt;
    GLenum type;
    const char *name;
};

struct gl_tex_impl_spec {
    int num_planes;
    const struct gl_tex_plane_spec *plane_specs;
    const char *shader_frag_source;
};

static const struct gl_tex_impl_spec tex_spec_unknown = {
    .num_planes = 0,
    .plane_specs = NULL,
    .shader_frag_source = NULL,
};

static const struct gl_tex_impl_spec tex_spec_rgba = {
    .num_planes = 1,
    .plane_specs = (const struct gl_tex_plane_spec[]) {
        { 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, "u_texture" },
    },
    .shader_frag_source =
        "precision mediump float;"
        "varying vec2 v_texture_pos;"
        "uniform sampler2D u_texture;"
        "void main() {"
        "    gl_FragColor = texture2D(u_texture, v_texture_pos);"
        "}",
};

static const struct gl_tex_impl_spec tex_spec_yuv420 = {
    .num_planes = 3,
    .plane_specs = (const struct gl_tex_plane_spec[]) {
        { 1, 1, GL_ALPHA, GL_UNSIGNED_BYTE, "u_texture_y" },
        { 1, 2, GL_ALPHA, GL_UNSIGNED_BYTE, "u_texture_u" },
        { 1, 2, GL_ALPHA, GL_UNSIGNED_BYTE, "u_texture_v" },
    },
    .shader_frag_source =
        "precision mediump float;"
        "varying vec2 v_texture_pos;"
        "uniform sampler2D u_texture_y;"
        "uniform sampler2D u_texture_u;"
        "uniform sampler2D u_texture_v;"
        "const vec3 c_yuv_offset = vec3(-0.0627451017, -0.501960814, -0.501960814);"
        "const mat3 c_yuv_matrix = mat3("
        "    1.1644,  1.1644,   1.1644,"
        "    0,      -0.2132,   2.1124,"
        "    1.7927, -0.5329,   0"
        ");"
        "void main() {"
        "    mediump vec3 yuv = vec3("
        "        texture2D(u_texture_y, v_texture_pos).a,"
        "        texture2D(u_texture_u, v_texture_pos).a,"
        "        texture2D(u_texture_v, v_texture_pos).a"
        "    );"
        "    lowp vec3 rgb = c_yuv_matrix * (yuv + c_yuv_offset);"
        "    gl_FragColor = vec4(rgb, 1);"
        "}",
};

struct gl_draw_tex_program {
    GLuint program;
    GLuint shader_vert;
    GLuint shader_frag;
    GLuint uniform_textures[MP_MAX_PLANES];
};

struct priv_platform {
    GLFWwindow *window;
};

struct priv_render {
    struct gl_draw_tex_program program_draw_tex_rgba;
    struct gl_draw_tex_program program_draw_tex_yuv420;
    void *buffer;
};

struct ui_texture {
    GLuint ids[MP_MAX_PLANES];
    int w;
    int h;
    enum ui_tex_fmt fmt;
};

static const struct gl_tex_impl_spec *get_gl_tex_impl_spec(enum ui_tex_fmt fmt)
{
    switch (fmt) {
    case TEX_FMT_RGBA:
        return &tex_spec_rgba;
    case TEX_FMT_YUV420:
        return &tex_spec_yuv420;
    case TEX_FMT_UNKNOWN:
        return &tex_spec_unknown;
    }
    return &tex_spec_unknown;
}

static struct gl_draw_tex_program *get_gl_draw_tex_program(struct priv_render *priv, enum ui_tex_fmt fmt)
{
    switch (fmt) {
    case TEX_FMT_RGBA:
        return &priv->program_draw_tex_rgba;
    case TEX_FMT_YUV420:
        return &priv->program_draw_tex_yuv420;
    case TEX_FMT_UNKNOWN:
        return NULL;
    }
    return NULL;
}

static struct priv_platform *get_priv_platform(struct ui_context *ctx)
{
    return (struct priv_platform*) ctx->priv_platform;
}

static struct priv_render *get_priv_render(struct ui_context *ctx)
{
    return (struct priv_render*) ctx->priv_render;
}

static void get_glfw_centered_window_pos(GLFWwindow *win, int *out_x, int *out_y)
{
    int monitor_count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
    if (monitor_count <= 0)
        return;

    int monitor_x = 0;
    int monitor_y = 0;
    glfwGetMonitorPos(monitors[0], &monitor_x, &monitor_y);

    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(win, &win_w, &win_h);

    const GLFWvidmode *mode = glfwGetVideoMode(monitors[0]);
    *out_x = monitor_x + (mode->width - win_w) / 2;
    *out_y = monitor_y + (mode->height - win_h) / 2;
}

static void on_window_close(GLFWwindow *window)
{
    void *ctx = glfwGetWindowUserPointer(window);
    ui_request_mpv_shutdown(ctx);
}

static bool platform_init(struct ui_context *ctx)
{
    // do not bother with audio output implementation, just reuse it
    audio_out_vita = audio_out_alsa;

    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(VITA_SCREEN_W, VITA_SCREEN_H, "Vita", NULL, NULL);
    if (!window)
        return false;

    int win_pos_x = 0;
    int win_pos_y = 0;
    get_glfw_centered_window_pos(window, &win_pos_x, &win_pos_y);

    glfwDefaultWindowHints();
    glfwSetWindowPos(window, win_pos_x, win_pos_y);
    glfwShowWindow(window);
    glfwSwapInterval(0);
    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, ctx);
    glfwSetWindowCloseCallback(window, on_window_close);

    struct priv_platform *priv = get_priv_platform(ctx);
    priv->window = window;
    return true;
}

static void platform_uninit(struct ui_context *ctx)
{
    struct priv_platform *priv = get_priv_platform(ctx);
    if (priv->window)
        glfwDestroyWindow(priv->window);
    glfwTerminate();
}

static void platform_poll_events(struct ui_context *ctx)
{
    glfwPollEvents();
}

static void delete_program(struct gl_draw_tex_program *program)
{
    if (program->program) {
        glDeleteProgram(program->program);
        program->program = 0;
    }
    if (program->shader_vert) {
        glDeleteShader(program->shader_vert);
        program->shader_vert = 0;
    }
    if (program->shader_frag) {
        glDeleteShader(program->shader_frag);
        program->shader_frag = 0;
    }
}

static bool load_shader(const char *source, GLenum type, GLuint *out_shader)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return false;
    }

    *out_shader = shader;
    return true;
}

static bool init_program(struct gl_draw_tex_program *program, const struct gl_tex_impl_spec *spec)
{
    bool succeed = true;
    succeed &= load_shader(shader_vert_source, GL_VERTEX_SHADER, &program->shader_vert);
    succeed &= load_shader(spec->shader_frag_source, GL_FRAGMENT_SHADER, &program->shader_frag);
    if (!succeed)
        goto error;

    program->program = glCreateProgram();
    glAttachShader(program->program, program->shader_vert);
    glAttachShader(program->program, program->shader_frag);
    glBindAttribLocation(program->program, attr_draw_tex_pos_draw.pos, attr_draw_tex_pos_draw.name);
    glBindAttribLocation(program->program, attr_draw_tex_pos_tex.pos, attr_draw_tex_pos_tex.name);
    glLinkProgram(program->program);

    GLint linked = 0;
    glGetProgramiv(program->program, GL_LINK_STATUS, &linked);
    if (!linked)
        goto error;

    for (int i = 0; i < spec->num_planes; ++i)
        program->uniform_textures[i] = glGetUniformLocation(program->program, spec->plane_specs[i].name);

    return true;

error:
    delete_program(program);
    return false;
}

static bool render_init(struct ui_context *ctx)
{
    struct priv_render *priv = ctx->priv_render;
    memset(priv, 0, sizeof(struct priv_render));
    return init_program(&priv->program_draw_tex_rgba, &tex_spec_rgba)
        && init_program(&priv->program_draw_tex_yuv420, &tex_spec_yuv420)
        && (priv->buffer = malloc(TEXTURE_BUFFER_SIZE));
}

static void render_uninit(struct ui_context *ctx)
{
    struct priv_render *priv = ctx->priv_render;
    delete_program(&priv->program_draw_tex_rgba);
    delete_program(&priv->program_draw_tex_yuv420);
    if (priv->buffer) {
        free(priv->buffer);
        priv->buffer = NULL;
    }
}

static void render_render_start(struct ui_context *ctx)
{
    glViewport(0, 0, VITA_SCREEN_W, VITA_SCREEN_H);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0, 0, 0, 0);
}

static void render_render_end(struct ui_context *ctx)
{
    struct priv_platform *priv_platform = get_priv_platform(ctx);
    glfwSwapBuffers(priv_platform->window);
}

static bool render_texture_init(struct ui_context *ctx, struct ui_texture **tex,
                                enum ui_tex_fmt fmt, int w, int h)
{
    struct ui_texture *new_tex = malloc(sizeof(struct ui_texture));
    new_tex->w = w;
    new_tex->h = h;
    new_tex->fmt = fmt;

    const struct gl_tex_impl_spec *spec = get_gl_tex_impl_spec(fmt);
    for (int i = 0; i < spec->num_planes; ++i) {
        const struct gl_tex_plane_spec *plane = spec->plane_specs + i;
        int tex_w = w / plane->div;
        int tex_h = h / plane->div;
        GLenum *p_tex_id = new_tex->ids + i;
        glGenTextures(1, p_tex_id);
        glBindTexture(GL_TEXTURE_2D, *p_tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, plane->fmt, tex_w, tex_h, 0, plane->fmt, plane->type, NULL);
    }

    *tex = new_tex;
    return true;
}

static void render_texture_uninit(struct ui_context *ctx, struct ui_texture **tex)
{
    const struct gl_tex_impl_spec *spec = get_gl_tex_impl_spec((*tex)->fmt);
    if (spec->num_planes > 0)
        glDeleteTextures(spec->num_planes, (*tex)->ids);
    *tex = NULL;
}

static void upload_texture_buffered(GLuint id, void *data, int w, int h, int stride, int bpp,
                                    GLenum fmt, GLenum type, void *buffer, int capacity)
{
    glBindTexture(GL_TEXTURE_2D, id);

    int row = 0;
    int col = 0;
    uint8_t *cur = data;
    uint8_t *next = cur + stride;
    int row_bytes = w * bpp;
    while (row < h) {
        // caculate readable bytes in current row
        int available_bytes = capacity;
        int read_bytes = MPMIN((w - col) * bpp, available_bytes);
        int read_pixels = read_bytes / bpp;
        if (read_bytes == 0) {
            // unless the buffer is too small
            return;
        }

        // texture upload area
        int dst_x = col;
        int dst_y = row;
        int dst_w = read_pixels;
        int dst_h = 1;

        uint8_t *dst_p = buffer;
        memcpy(dst_p, cur, read_bytes);
        cur += read_bytes;
        col += read_pixels;
        dst_p += read_bytes;
        available_bytes -= read_bytes;

        if (col == w) {
            // swith to next row
            ++row;
            col = 0;
            cur = next;
            next += stride;

            // copy as many rows as we can
            if (dst_x == 0) {
                int row_count = MPMIN((available_bytes / row_bytes), (h - row));
                for (int i = 0; i < row_count; ++i) {
                    memcpy(dst_p, cur, row_bytes);
                    ++row;
                    ++dst_h;
                    cur = next;
                    next += stride;
                    dst_p += row_bytes;
                }
            }
        }

        // finish current batch
        glTexSubImage2D(GL_TEXTURE_2D, 0, dst_x, dst_y, dst_w, dst_h, fmt, type, buffer);
    }
}

static void render_texture_upload(struct ui_context *ctx, struct ui_texture *tex,
                                  void **data, int *strides, int planes)
{
    const struct gl_tex_impl_spec *spec = get_gl_tex_impl_spec(tex->fmt);
    if (spec->num_planes != planes)
        return;

    struct priv_render *priv_render = ctx->priv_render;
    for (int i = 0; i < planes; ++i) {
        const struct gl_tex_plane_spec *plane = spec->plane_specs + i;
        int tex_w = tex->w / plane->div;
        int tex_h = tex->h / plane->div;
        upload_texture_buffered(tex->ids[i], data[i], tex_w, tex_h,
                                strides[i], plane->bpp, plane->fmt, plane->type,
                                priv_render->buffer, TEXTURE_BUFFER_SIZE);
    }
}

static void render_texture_draw(struct ui_context *ctx, struct ui_texture *tex,
                                float x, float y, float sx, float sy)
{
    const struct gl_tex_impl_spec *spec = get_gl_tex_impl_spec(tex->fmt);
    if (!spec)
        return;

    struct priv_render *priv = get_priv_render(ctx);
    struct gl_draw_tex_program *program = get_gl_draw_tex_program(priv, tex->fmt);
    if (!program)
        return;

    const GLfloat draw_vertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
         1.0f, -1.0f,
    };

    const GLfloat tex_vertices[] = {
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        1.0, 1.0,
    };

    glUseProgram(program->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->ids[0]);

    glVertexAttribPointer(attr_draw_tex_pos_draw.pos, 2, GL_FLOAT, GL_FALSE, 0, draw_vertices);
    glEnableVertexAttribArray(attr_draw_tex_pos_draw.pos);
    glVertexAttribPointer(attr_draw_tex_pos_tex.pos, 2, GL_FLOAT, GL_FALSE, 0, tex_vertices);
    glEnableVertexAttribArray(attr_draw_tex_pos_tex.pos);

    for (int i = 0; i < spec->num_planes; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, tex->ids[i]);
        glUniform1i(program->uniform_textures[i], i);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

const struct ui_platform_driver ui_platform_driver_vita = {
    .priv_size = sizeof(struct priv_platform),
    .init = platform_init,
    .uninit = platform_uninit,
    .poll_events = platform_poll_events,
};

const struct ui_render_driver ui_render_driver_vita = {
    .priv_size = sizeof(struct priv_render),

    .init = render_init,
    .uninit = render_uninit,

    .render_start = render_render_start,
    .render_end = render_render_end,

    .texture_init = render_texture_init,
    .texture_uninit = render_texture_uninit,
    .texture_upload = render_texture_upload,
    .texture_draw = render_texture_draw,
};
