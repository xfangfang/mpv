#pragma once

#include <stdbool.h>
#include <psp2/gxm.h>

#include "video/out/gpu/ra.h"
#include "video/out/gpu/spirv.h"

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

//// Flush the immediate context of the wrapped D3D11 device
//void ra_d3d11_flush(struct ra *ra);
//
//// Create an RA texture from a D3D11 resource. This takes a reference to the
//// texture, which is released when the RA texture is destroyed.
struct ra_tex *ra_gxm_wrap_tex(struct ra *ra);

//// As above, but for a D3D11VA video resource. The fmt parameter selects which
//// plane of a planar format will be mapped when the RA texture is used.
//// array_slice should be set for texture arrays and is ignored for non-arrays.
//struct ra_tex *ra_d3d11_wrap_tex_video(struct ra *ra, ID3D11Texture2D *res,
//                                       int w, int h, int array_slice,
//                                       const struct ra_format *fmt);
//
//// Get the underlying D3D11 resource from an RA texture. The returned resource
//// is refcounted and must be released by the caller.
//ID3D11Resource *ra_d3d11_get_raw_tex(struct ra *ra, struct ra_tex *tex,
//                                     int *array_slice);
//
//// Get the underlying D3D11 device from an RA instance. The returned device is
//// refcounted and must be released by the caller.
//ID3D11Device *ra_d3d11_get_device(struct ra *ra);

// True if the RA instance was created with ra_gxm_create()
bool ra_is_gxm(struct ra *ra);
