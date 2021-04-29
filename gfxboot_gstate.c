#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// gstate


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_gstate_new()
{
  return gfx_obj_alloc(OTYPE_GSTATE, OBJ_GSTATE_SIZE());
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
gstate_t *gfx_obj_gstate_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_GSTATE) return 0;

  return (gstate_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_gstate_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  gstate_t *gstate = ptr->data.ptr;
  unsigned len = ptr->data.size;
  if(len != OBJ_GSTATE_SIZE()) {
    if(style.ref) {
      gfxboot_log("      <invalid data>\n");
    }
    else {
      gfxboot_log("<invalid data>");
    }

    return 1;
  }

  if(!style.ref) {
    if(!style.inspect) return 0;

    gfxboot_log("region %dx%d_%dx%d", gstate->region.x, gstate->region.y, gstate->region.width, gstate->region.height);

    return 1;
  }

  if(style.dump) {
    int width = 0;
    int height = 0;
    canvas_t *canvas = gfx_obj_canvas_ptr(gstate->canvas_id);

    if(canvas) {
      width = canvas->width;
      height = canvas->height;
    }

    gfxboot_log("    pos %dx%d", gstate->pos.x, gstate->pos.y);
    if(gstate->pos.width || gstate->pos.height) {
      gfxboot_log(", char size %dx%d", gstate->pos.width, gstate->pos.height);
    }
    gfxboot_log("\n    color #%08x, bg_color #%08x\n", gstate->color, gstate->bg_color);
    gfxboot_log("    canvas %s (%dx%d)\n", gfx_obj_id2str(gstate->canvas_id), width, height);
    gfxboot_log("    font %s\n", gfx_obj_id2str(gstate->font_id));
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_gstate_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  gstate_t *gstate = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned more_gc = 0;

  if(gstate && data_size == OBJ_GSTATE_SIZE()) {
    more_gc += gfx_obj_ref_dec_delay_gc(gstate->canvas_id);
    more_gc += gfx_obj_ref_dec_delay_gc(gstate->font_id);
  }

  return more_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_gstate_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  gstate_t *gstate = ptr->data.ptr;
  unsigned data_size = ptr->data.size;

  if(gstate && data_size == OBJ_GSTATE_SIZE()) {
    if(id == gstate->canvas_id || id == gstate->font_id) return 1;
  }

  return 0;
}
