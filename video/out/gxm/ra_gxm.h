#pragma once

#include <stdbool.h>
#include <psp2/gxm.h>

#include "video/out/gpu/ra.h"
#include "video/out/gpu/spirv.h"

struct ra_gxm {
    SceGxmContext *context;
    SceGxmShaderPatcher *shader_patcher;
    // uniform buffer index
    int buffer_index;
};

struct ra_tex_gxm {
    SceGxmTexture gxm_tex;
    uint8_t *tex_data;
    SceUID data_UID;
    SceGxmTextureFormat format;
};

struct gxm_format {
    const char *name;
    int components;
    int bytes;
    int bits[4];
    SceGxmTextureFormat format;
    enum ra_ctype ctype;
    bool renderable, linear_filter, storable, ordered;
};

//// Get the underlying DXGI format from an RA format
//DXGI_FORMAT ra_d3d11_get_format(const struct ra_format *fmt);
//
//// Gets the matching ra_format for a given DXGI format.
//// Returns a nullptr in case of no known match.
//const struct ra_format *ra_d3d11_get_ra_format(struct ra *ra, DXGI_FORMAT fmt);

// Create an RA instance from a D3D11 device. This takes a reference to the
// device, which is released when the RA instance is destroyed.
struct ra *ra_gxm_create(SceGxmContext *context, SceGxmShaderPatcher *shader_patcher, struct mp_log *log, int buffer_index);

// True if the RA instance was created with ra_gxm_create()
bool ra_is_gxm(struct ra *ra);
