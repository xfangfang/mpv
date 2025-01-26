#include <libmpv/client.h>
#include <libmpv/render_gxm.h>
#include <libmpv/render.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <vita2d.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/clib.h>

#include "config.h"

#if HAVE_VITASHARK
#include <vitashark.h>
#endif

#define printf sceClibPrintf

unsigned int _pthread_stack_default_user = 2 * 1024 * 1024;

static int redraw = 0;

static void mpv_render_update(void *cb_ctx) {
    redraw = 1;
}

int main(int argc, char *argv[]) {
    printf("==== START ====\n");

#if HAVE_VITASHARK
    if (shark_init(NULL) < 0) {
        sceClibPrintf("vitashark: failed to initialize\n");
        return EXIT_FAILURE;
    }
#endif

    SceCtrlData pad, old_pad;
    unsigned int pressed;
    vita2d_init();
    vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));

    mpv_handle *mpv = mpv_create();
    if (!mpv) {
        printf("failed to create mpv context\n");
        return EXIT_FAILURE;
    }

    printf("Initialize mpv render context\n");
    mpv_gxm_init_params gxm_params = {
            .context = vita2d_get_context(),
            .shader_patcher = vita2d_get_shader_patcher(),
            .buffer_index = 0,
            .msaa = SCE_GXM_MULTISAMPLE_NONE,
    };

    mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE,        (void *) MPV_RENDER_API_TYPE_GXM},
            {MPV_RENDER_PARAM_GXM_INIT_PARAMS, &gxm_params},
            {0}
    };
    mpv_render_context *mpv_context;
    if (mpv_render_context_create(&mpv_context, mpv, params) < 0) {
        printf("failed to create mpv render context\n");
        return EXIT_FAILURE;
    }
    printf("Set update callback\n");
    mpv_render_context_set_update_callback(mpv_context, mpv_render_update, NULL);

    printf("Set mpv options\n");
    mpv_set_option_string(mpv, "terminal", "yes");
    mpv_set_option_string(mpv, "msg-level", "all=debug");
    mpv_set_option_string(mpv, "vd-lavc-threads", "4");
    mpv_set_option_string(mpv, "fbo-format", "rgba8");
    mpv_set_option_string(mpv, "hwdec", "auto");

    // Put font file to ux0:/data/fonts/ to test libass
    mpv_set_option_string(mpv, "osd-fonts-dir", "ux0:/data/fonts");
    mpv_set_option_string(mpv, "osd-font", "Open Sans");
    mpv_set_option_string(mpv, "osd-msg1", "libass text");


    printf("Initialize mpv\n");
    if (mpv_initialize(mpv) < 0) {
        printf("failed to initialize mpv\n");
        return EXIT_FAILURE;
    }
    {
        const char *cmd[] = {"set", "background", "#FFFF00", NULL};
        mpv_command(mpv, cmd);
    }

    int texture_width = 960;
    int texture_height = 544;
    vita2d_texture *img = vita2d_create_empty_texture_rendertarget(texture_width, texture_height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);

    int flip_y = 1;
    mpv_gxm_fbo fbo = {
            .render_target = img->gxm_rtgt,
            .color_surface = &img->gxm_sfc,
            .depth_stencil_surface = &img->gxm_sfd,
            .w = texture_width,
            .h = texture_height,
            .format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA,
    };
    mpv_render_param mpv_params[3] = {
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_GXM_FBO, &fbo},
            {MPV_RENDER_PARAM_INVALID, NULL},
    };

    {
        const char *cmd[] = {"loadfile", "file://ux0:/test.mp4", "replace", NULL};
//        const char *cmd[] = {"loadfile", "file://ux0:/sintel_trailer-720p.mp4", NULL};
        mpv_command(mpv, cmd);
    }

    memset(&pad, 0, sizeof(pad));
    memset(&old_pad, 0, sizeof(old_pad));
    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        pressed = pad.buttons & ~old_pad.buttons;
        old_pad = pad;

        if (pressed & SCE_CTRL_START)
            break;

        if (pressed & SCE_CTRL_CIRCLE) {
            static bool changed = true;
            const char *cmd[] = {"set", "pause", changed ? "yes" : "no", NULL};
            mpv_command(mpv, cmd);
            changed = !changed;
        }

        if (pressed & SCE_CTRL_TRIANGLE) {
            static bool changed = true;
            const char *cmd[] = {"set", "video-margin-ratio-right", changed ? "0.5" : "0.0", NULL};
            mpv_command(mpv, cmd);
            changed = !changed;
        }

        if (pressed & SCE_CTRL_SQUARE) {
            static bool changed = true;
            const char *cmd[] = {"set", "gamma", changed ? "100" : "0", NULL};
            mpv_command(mpv, cmd);
            changed = !changed;
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_draw_texture(img, 0, 0);

        vita2d_end_drawing();
        vita2d_swap_buffers();

        if (redraw && mpv_render_context_update(mpv_context) & MPV_RENDER_UPDATE_FRAME) {
            redraw = 0;
            mpv_render_context_render(mpv_context, mpv_params);
            mpv_render_context_report_swap(mpv_context);
        }
    }

    mpv_command_string(mpv, "quit");
    mpv_render_context_free(mpv_context);
    mpv_terminate_destroy(mpv);
    vita2d_fini();
    vita2d_free_texture(img);

#if HAVE_VITASHARK
    // Clean up vitashark as we don't need it anymore
    shark_end();
#endif
    return 0;
}
