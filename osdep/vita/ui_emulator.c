#include "ui.h"
#include "video/img_format.h"
#include "audio/out/internal.h"

#include <GLFW/glfw3.h>

#define TEXTURE_BUFFER_SIZE     (1024 * 1024 * 4)

extern const struct ao_driver audio_out_alsa;
struct ao_driver audio_out_vita;

enum draw_tex_attr {
    ATTR_DRAW_TEX_POS_DRAW,
    ATTR_DRAW_TEX_POS_TEX,
};

struct priv_platform {
    GLFWwindow *window;
};

struct priv_render {
    struct {
        GLuint program;
        GLuint shader_vert;
        GLuint shader_frag;
        GLuint u_texture;
    } program_draw_tex;

    void *buffer;
};

struct ui_texture {
    GLuint id;
    int w;
    int h;
    enum ui_tex_fmt fmt;
};

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

static void delete_program_checked(GLuint *program)
{
    if (program && *program) {
        glDeleteProgram(*program);
        *program = 0;
    }
}

static void delete_shader_checked(GLuint *shader)
{
    if (shader && *shader) {
        glDeleteShader(*shader);
        *shader = 0;
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

static bool render_init_programe_draw_tex(struct priv_render *priv)
{
    const char *shader_vert_code =
            "attribute vec4 a_draw_pos; \n"
            "attribute vec2 a_texture_pos; \n"
            "varying vec2 v_texture_pos; \n"
            "void main() { \n"
            "    gl_Position = a_draw_pos; \n"
            "    v_texture_pos = a_texture_pos; \n"
            "} \n";
    const char *shader_frag_code =
            "precision mediump float; \n"
            "varying vec2 v_texture_pos; \n"
            "uniform sampler2D u_texture; \n"
            "void main() { \n"
            "    gl_FragColor = texture2D(u_texture, v_texture_pos); \n"
            "} \n";

    bool succeed = true;
    GLuint program = 0;
    GLuint shader_vert = 0;
    GLuint shader_frag = 0;
    succeed &= load_shader(shader_vert_code, GL_VERTEX_SHADER, &shader_vert);
    succeed &= load_shader(shader_frag_code, GL_FRAGMENT_SHADER, &shader_frag);
    if (!succeed)
        goto error;

    program = glCreateProgram();
    glAttachShader(program, shader_vert);
    glAttachShader(program, shader_frag);
    glBindAttribLocation(program, ATTR_DRAW_TEX_POS_DRAW, "a_draw_pos");
    glBindAttribLocation(program, ATTR_DRAW_TEX_POS_TEX, "a_texture_pos");
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
        goto error;

    priv->program_draw_tex.program = program;
    priv->program_draw_tex.shader_vert = shader_vert;
    priv->program_draw_tex.shader_frag = shader_frag;
    priv->program_draw_tex.u_texture = glGetUniformLocation(program, "u_texture");
    return true;

error:
    delete_program_checked(&program);
    delete_shader_checked(&shader_vert);
    delete_shader_checked(&shader_frag);
    return false;
}

static bool render_init(struct ui_context *ctx)
{
    struct priv_render *priv = ctx->priv_render;
    return render_init_programe_draw_tex(priv) &&
           (priv->buffer = malloc(TEXTURE_BUFFER_SIZE));
}

static void render_uninit(struct ui_context *ctx)
{
    struct priv_render *priv = ctx->priv_render;
    delete_program_checked(&priv->program_draw_tex.program);
    delete_shader_checked(&priv->program_draw_tex.shader_vert);
    delete_shader_checked(&priv->program_draw_tex.shader_frag);

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
    glfwPollEvents();
}

static bool render_texture_init(struct ui_context *ctx, struct ui_texture **tex,
                                enum ui_tex_fmt fmt, int w, int h)
{
    struct ui_texture *new_tex = malloc(sizeof(struct ui_texture));
    new_tex->w = w;
    new_tex->h = h;
    new_tex->fmt = fmt;

    if (fmt == TEX_FMT_RGBA) {
        glGenTextures(1, &new_tex->id);
        glBindTexture(GL_TEXTURE_2D, new_tex->id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    *tex = new_tex;
    return true;
}

static void render_texture_uninit(struct ui_context *ctx, struct ui_texture **tex)
{
    if ((*tex)->fmt == TEX_FMT_RGBA) {
        glDeleteTextures(1, &(*tex)->id);
    }
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
    struct priv_render *priv_render = ctx->priv_render;
    if (tex->fmt == TEX_FMT_RGBA) {
        upload_texture_buffered(tex->id, data[0], tex->w, tex->h, strides[0], 4,
                                GL_RGBA, GL_UNSIGNED_BYTE,
                                priv_render->buffer, TEXTURE_BUFFER_SIZE);
    }
}

static void render_texture_draw(struct ui_context *ctx, struct ui_texture *tex,
                                float x, float y, float sx, float sy)
{
    if (tex->fmt != TEX_FMT_RGBA) {
        return;
    }

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

    struct priv_render *priv = get_priv_render(ctx);
    glUseProgram(priv->program_draw_tex.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glVertexAttribPointer(ATTR_DRAW_TEX_POS_DRAW, 2, GL_FLOAT, GL_FALSE, 0, draw_vertices);
    glEnableVertexAttribArray(ATTR_DRAW_TEX_POS_DRAW);
    glVertexAttribPointer(ATTR_DRAW_TEX_POS_TEX, 2, GL_FLOAT, GL_FALSE, 0, tex_vertices);
    glEnableVertexAttribArray(ATTR_DRAW_TEX_POS_TEX);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

const struct ui_platform_driver ui_platform_driver_vita = {
    .priv_size = sizeof(struct priv_platform),
    .init = platform_init,
    .uninit = platform_uninit,
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
