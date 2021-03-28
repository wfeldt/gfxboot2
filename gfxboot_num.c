#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// num


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_num_new(int64_t num, uint8_t sub_type)
{
  obj_id_t id = gfx_obj_new(OTYPE_NUM);
  obj_t *ptr = gfx_obj_ptr(id);

  if(ptr) {
    ptr->data.value = num;
    ptr->sub_type = sub_type;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int64_t *gfx_obj_num_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_NUM) return 0;

  return &ptr->data.value;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int64_t *gfx_obj_num_subtype_ptr(obj_id_t id, uint8_t subtype)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_NUM || ptr->sub_type != subtype) return 0;

  return &ptr->data.value;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_num_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  int64_t num = ptr->data.value;

  if(style.ref) return 1;

  switch(style.inspect) {
    case 0:
      gfxboot_log("%lld", (long long) num);
      break;
    case 1:
      gfxboot_log("%lld (0x%llx)", (long long) num, (long long) num);
      break;
  }

  return 1;
}


