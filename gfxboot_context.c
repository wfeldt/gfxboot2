#include <gfxboot/gfxboot.h>
#include <gfxboot/vocabulary.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// context


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_context_new(uint8_t sub_type)
{
  obj_id_t id = gfx_obj_alloc(OTYPE_CONTEXT, OBJ_CONTEXT_SIZE());
  obj_t *ptr = gfx_obj_ptr(id);

  if(ptr) {
    ptr->sub_type = sub_type;
    ((context_t *) ptr->data.ptr)->type = sub_type;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
context_t *gfx_obj_context_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_CONTEXT) return 0;

  return (context_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_context_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  context_t *context = ptr->data.ptr;
  unsigned len = ptr->data.size;
  if(len != OBJ_CONTEXT_SIZE()) {
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

    gfxboot_log("code %s, ip 0x%x (0x%x)", gfx_obj_id2str(context->code_id), context->ip, context->current_ip);

    if(context->type == t_ctx_repeat) {
      gfxboot_log(", index %lld", (long long) context->index);
    }
    else if(context->type == t_ctx_for) {
      gfxboot_log(
        ", index %lld, inc %lld, max %lld",
        (long long) context->index,
        (long long) context->inc,
        (long long) context->max
      );
    }
    else if(context->type == t_ctx_forall) {
      gfxboot_log(
        ", index %lld, iterate %s",
        (long long) context->index,
        gfx_obj_id2str(context->iterate_id)
      );
    }

    if(context->dict_id) gfxboot_log(", dict %s", gfx_obj_id2str(context->dict_id));

    return 1;
  }

  if(style.dump) {
    gfxboot_log("    type %u, ip 0x%x (0x%x)\n", context->type, context->ip, context->current_ip);
    gfxboot_log("    code %s\n", gfx_obj_id2str(context->code_id));
    gfxboot_log("    parent %s\n", gfx_obj_id2str(context->parent_id));
    gfxboot_log("    dict %s\n", gfx_obj_id2str(context->dict_id));
    gfxboot_log("    iterate %s\n", gfx_obj_id2str(context->iterate_id));
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_context_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  context_t *context = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned more_gc = 0;

  if(context && data_size == OBJ_CONTEXT_SIZE()) {
    more_gc += gfx_obj_ref_dec_delay_gc(context->parent_id);
    more_gc += gfx_obj_ref_dec_delay_gc(context->code_id);
    more_gc += gfx_obj_ref_dec_delay_gc(context->dict_id);
    more_gc += gfx_obj_ref_dec_delay_gc(context->iterate_id);
  }

  return more_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_context_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  context_t *context = ptr->data.ptr;
  unsigned data_size = ptr->data.size;

  if(context && data_size == OBJ_CONTEXT_SIZE()) {
    if(
      id == context->parent_id ||
      id == context->code_id ||
      id == context->dict_id ||
      id == context->iterate_id
    ) return 1;
  }

  return 0;
}
