#include "sub/osd.h"
#include "sub/osd_state.h"

void osd_init_backend(struct osd_state *osd) {}

void osd_destroy_backend(struct osd_state *osd) {}

void osd_set_external(struct osd_state *osd, struct osd_external_ass *ov) {}

void osd_set_external_remove_owner(struct osd_state *osd, void *owner) {}

void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function)
{
    if (buffer_size > 2) {
        buffer[0] = (char)osd_function;
        buffer[1] = 0;
    }
}

void osd_get_text_size(struct osd_state *osd, int *out_screen_h, int *out_font_h)
{
    //TODO
}

struct sub_bitmaps *osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj, int format)
{
    //TODO
    return NULL;
}
