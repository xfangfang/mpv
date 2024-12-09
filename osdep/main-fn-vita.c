#include <libmpv/client.h>
#include <libmpv/render_gxm.h>
#include <libmpv/render.h>
#include <stdbool.h>

#include <nanovg.h>
#define NANOVG_GXM_IMPLEMENTATION
#define NANOVG_GXM_UTILS_IMPLEMENTATION
#include <nanovg_gxm.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/clib.h>

#define printf sceClibPrintf

unsigned int _newlib_heap_size_user      = 220 * 1024 * 1024;
unsigned int sceLibcHeapSize             = 24 * 1024 * 1024;
unsigned int _pthread_stack_default_user = 2 * 1024 * 1024;

static int redraw = 0;

static void mpv_render_update(void *cb_ctx) {
    redraw = 1;
}

int main(int argc, char *argv[]) {
    printf("==== START ====\n");

#ifdef USE_VITA_SHARK
    if (shark_init(NULL) < 0) {
        sceClibPrintf("vitashark: failed to initialize\n");
        return EXIT_FAILURE;
    }
#endif

    SceCtrlData pad;
    NVGXMframebuffer *gxm = NULL;
    NVGcontext        *vg = NULL;
    NVGXMinitOptions initOptions = {
            .msaa = SCE_GXM_MULTISAMPLE_4X,
            .swapInterval = 1,
            .dumpShader = 0,
    };

    gxm = nvgxmCreateFramebuffer(&initOptions);
    if (gxm == NULL) {
        sceClibPrintf("gxm: failed to initialize\n");
        return EXIT_FAILURE;
    }

    vg = nvgCreateGXM(gxm, NVG_DEBUG);
    if (vg == NULL) {
        sceClibPrintf("nanovg: failed to initialize\n");
        return EXIT_FAILURE;
    }

    mpv_handle *mpv = mpv_create();
    if (!mpv) {
        printf("failed to create mpv context\n");
        return EXIT_FAILURE;
    }

    printf("Initialize mpv render context\n");
    mpv_gxm_init_params gxm_params = {
            .context = gxm->context,
            .shader_patcher = gxm->shader_patcher,
            .buffer_index = 2,
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

//    mpv_set_option_string(mpv, "scale", "mitchell");
//    mpv_set_option_string(mpv, "dscale", "bilinear");

    printf("Initialize mpv\n");
    if (mpv_initialize(mpv) < 0) {
        printf("failed to initialize mpv\n");
        return EXIT_FAILURE;
    }
    {
        const char *cmd[] = {"set", "background", "#FFFF00", NULL};
        mpv_command(mpv, cmd);
    }


    int flip_y = 1;
    mpv_render_param mpv_params[2] = {
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, NULL},
    };

    printf("Load a file\n");
    {
        const char *cmd[] = {"loadfile", "file://ux0:/test.mp4", "replace", "vf=hflip", NULL};
        mpv_command(mpv, cmd);
    }

    NVGcolor clearColor = nvgRGBAf(0.3f, 0.3f, 0.32f, 1.0f);
    gxmClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        if (pad.buttons & SCE_CTRL_CIRCLE) {
            static bool paused = false;
            const char *cmd[] = {"set", "pause", paused ? "no" : "yes", NULL};
            mpv_command(mpv, cmd);
            paused = !paused;
        }

        if (pad.buttons & SCE_CTRL_TRIANGLE) {
            const char *cmd[] = {"set", "video-margin-ratio-right", "0.5", NULL};
            mpv_command(mpv, cmd);
        }

        gxmBeginFrame();
        gxmClear();
        nvgBeginFrame(vg, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1.0f);

        if (redraw && mpv_render_context_update(mpv_context) & MPV_RENDER_UPDATE_FRAME) {
            redraw = 0;
            mpv_render_context_render(mpv_context, mpv_params);
        }

        nvgEndFrame(vg);
        gxmEndFrame();
        gxmSwapBuffer();
        mpv_render_context_report_swap(mpv_context);
    }

    mpv_command_string(mpv, "quit");
    mpv_render_context_free(mpv_context);
    mpv_terminate_destroy(mpv);
    nvgDeleteGXM(vg);
    nvgxmDeleteFramebuffer(gxm);

#ifdef USE_VITA_SHARK
    // Clean up vitashark as we don't need it anymore
    shark_end();
#endif
    return 0;
}
