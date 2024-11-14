#include "main-fn.h"

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


int main(int argc, char *argv[])
{
    printf("Hello, world!\n");
    const char *args[] = {
            "mpv",
            "--vo=null",
            "file://ux0:/test.mp4",
            ""
    };
    mpv_main(1, args);

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

    vg = nvgCreateGXM(gxm, NVG_STENCIL_STROKES);
    if (vg == NULL) {
        sceClibPrintf("nanovg: failed to initialize\n");
        return EXIT_FAILURE;
    }

    NVGcolor clearColor = nvgRGBAf(0.3f, 0.3f, 0.32f, 1.0f);
    gxmClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        gxmBeginFrame();
        gxmClear();
        nvgBeginFrame(vg, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1.0f);

        nvgBeginPath(vg);
        nvgRect(vg, 100, 100, 120, 30);
        nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
        nvgFill(vg);

        nvgEndFrame(vg);
        gxmEndFrame();
        gxmSwapBuffer();
    }

    return 0;
}
