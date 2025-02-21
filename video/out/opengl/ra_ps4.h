#pragma once

#include <stdbool.h>
#include <orbis/libkernel.h>
#include <orbis/Pigletv2VSH.h>

typedef struct ps4_shader {
    char sha[65];
    const char *src;
    int size;
} ps4_shader;

extern ps4_shader ps4_shaders_list[];
extern int ps4_shaders_list_size;

extern int ps4_mpv_use_precompiled_shaders;
extern int ps4_mpv_dump_shaders;

void glPigletGetShaderBinarySCE(
    GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary);

ps4_shader *ps4_mpv_get_shader(const char *name);

int ps4_mpv_get_shader_hash(const char *prog, size_t len, char *out);

void ps4_mpv_dump_shader(GLuint id, const char *path, const char *name);

// default shader
extern const char ps4_vert_default[];
extern const char ps4_frag_default[];
#define ps4_vert_default_sha "0343CB3E07C78C9502D7DC28FE81B01156659FAAD74DAFF538CA814D22C54934"
#define ps4_frag_default_sha "3656E3A040C0B5241A6B93FF03F59A793B0EF6F20D6F590F7C84D569BCB025EA"

// default shader with gamma correction
extern const char ps4_frag_default_gamma[];
#define ps4_frag_default_gamma_sha "157F8A13AE02FA8D5D4C1AA15C5A76936A837BE8EC27137FD2F7E4C9AC3697C3"

// subtitle default shader
extern const char ps4_vert_sub[];
extern const char ps4_frag_sub[];
#define ps4_vert_sub_sha "C8F6C1D3AB0F60216199E37E11598772306FE946A7790A9F8223478ACEAB24C5"
#define ps4_frag_sub_sha "148634E777E2C3006104DB8FF2F97A8C486DE5C32A8B88479E5AA8B61571BB49"

// ...
extern const char ps4_frag_59EAFD[];
#define ps4_frag_59EAFD_sha "59EAFD8B6C03161A000079B3A3DB5831CBC0F6874BCE8C676B5372B22B30A891"

