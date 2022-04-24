#pragma once

#include "misc/dispatch.h"

struct ui_context {
    void *priv_platform;
    void *priv_render;
    void *priv_panel;
    void *priv_context;
    struct mp_dispatch_queue *dispatch;
};
