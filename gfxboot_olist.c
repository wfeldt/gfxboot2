#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// obj list


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_olist_new(unsigned max)
{
  obj_id_t id = gfx_obj_alloc(OTYPE_OLIST, OBJ_OLIST_SIZE(max));
  olist_t *ol = gfx_obj_olist_ptr(id);

  if(ol) {
    ol->max = max;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
olist_t *gfx_obj_olist_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_OLIST) return 0;

  return (olist_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_olist_dump(obj_t *ptr, dump_style_t style)
{
  unsigned u, used;

  if(!ptr) return 1;

  olist_t *ol = ptr->data.ptr;

  if(ptr->data.size != OBJ_OLIST_SIZE(ol->max)) {
    gfxboot_log("      <invalid data>\n");

    return 1;
  }

  for(u = used = 0; u < ol->max; u++) {
    if(ol->ptr[u].base_type != OTYPE_NONE) used++;
  }

  if(!style.ref) {
    if(style.inspect) {
      gfxboot_log("size %u, next %u, max %u", used, ol->next, ol->max);

      return 1;
    }
    else {
      return 0;
    }
  }

  if(style.dump) gfxboot_log("  ");

  for(u = 0; u < ol->max; u++) {
    if(ol->ptr[u].base_type != OTYPE_NONE) {
      obj_id_t id = OBJ_ID(u, ol->ptr[u].gen);
      dump_style_t s = { .inspect = style.inspect, .no_head = 1 };
      s.dump = style.dump && u;
      gfx_obj_dump(id, s);
    }
  }

  return 1;
}
