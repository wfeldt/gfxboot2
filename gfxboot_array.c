#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// array

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_new(unsigned max)
{
  if(!max) max = 0x10;

  obj_id_t id = gfx_obj_alloc(OTYPE_ARRAY, OBJ_ARRAY_SIZE(max));
  array_t *a = gfx_obj_array_ptr(id);

  if(a) {
    a->max = max;
    a->size = 0;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
array_t *gfx_obj_array_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_ARRAY) return 0;

  return (array_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
array_t *gfx_obj_array_ptr_rw(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_ARRAY) return 0;

  if(ptr->flags.ro) {
    GFX_ERROR(err_readonly);
    return 0;
  }

  return (array_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_array_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2)
{
  array_t *a = ptr->data.ptr;

  if(ptr->data.size != OBJ_ARRAY_SIZE(a->max)) {
    GFX_ERROR(err_internal);
    return 0;
  }

  if(*idx >= a->size) {
    return 0;
  }

  gfx_obj_ref_inc(*id1 = a->ptr[*idx]);
  (*idx)++;

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_array_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  array_t *a = ptr->data.ptr;
  unsigned u;
  obj_id_t id;

  if(ptr->data.size != OBJ_ARRAY_SIZE(a->max)) {
    gfxboot_log("<invalid array>");
    if(style.ref) gfxboot_log("\n");

    return 1;
  }

  if(!style.ref) {
    if(!style.inspect) return 0;

    gfxboot_log("size %u, max %u", a->size, a->max);

    return 1;
  }

  for(u = 0; u < a->size && (!style.max || u < style.max); u++) {
    id = a->ptr[u];
    if(style.dump) gfxboot_log("    ");
    gfxboot_log("[%2u] ", u);
    gfx_obj_dump(id, (dump_style_t) { .inspect = style.inspect, .no_nl = 1 });
    gfxboot_log("\n");
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_set(obj_id_t array_id, obj_id_t id, int pos, int do_ref_cnt)
{
  array_t *a = gfx_obj_array_ptr_rw(array_id);

  if(!a) return 0;

  if(pos < 0) pos = (int) a->size + pos;

  if(pos < 0) return 0;

  unsigned upos = (unsigned) pos;

  if(upos >= a->max) {
    unsigned max = upos + (upos >> 3) + 0x10;
    array_id = gfx_obj_realloc(array_id, OBJ_ARRAY_SIZE(max));
    if(!array_id) return 0;
    a = gfx_obj_array_ptr(array_id);
    if(!a) return 0;
    a->max = max;
  }

  if(upos >= a->size) {
    unsigned u;

    // clear new entries
    for(u = a->size; u <= upos; u++) a->ptr[u] = 0;
    a->size = upos + 1;
  }

  if(do_ref_cnt) {
    gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(a->ptr[upos]);
  }
  a->ptr[upos] = id;

  return array_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_insert(obj_id_t array_id, obj_id_t id, int pos, int do_ref_cnt)
{
  array_t *a = gfx_obj_array_ptr_rw(array_id);

  if(!a) return 0;

  if(pos < 0) pos = (int) a->size + pos;

  if(pos < 0) return 0;

  unsigned upos = (unsigned) pos;

  unsigned max = a->max;

  if(upos >= max) {
    max = upos + (upos >> 3) + 0x10;
  }
  else if(a->size + 1 > max) {
    max = max + (max >> 3) + 0x10;
  }

  if(max > a->max) {
    array_id = gfx_obj_realloc(array_id, OBJ_ARRAY_SIZE(max));
    if(!array_id) return 0;
    a = gfx_obj_array_ptr(array_id);
    if(!a) return 0;
    a->max = max;
  }

  if(upos >= a->size) {
    // clear new entries
    for(unsigned u = a->size; u <= upos; u++) a->ptr[u] = 0;
    a->size = upos + 1;
  }
  else {
    gfx_memcpy(&a->ptr[upos + 1], &a->ptr[upos], (sizeof *a->ptr) * (a->size - upos));
    a->size++;
    a->ptr[upos] = 0;
  }

  if(do_ref_cnt) {
    gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(a->ptr[upos]);
  }
  a->ptr[upos] = id;

  return array_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_get(obj_id_t array_id, int pos)
{
  array_t *a = gfx_obj_array_ptr(array_id);

  if(!a) return 0;

  if(pos < 0) pos = (int) a->size + pos;

  if(pos < 0 || pos >= (int) a->size) return 0;

  return a->ptr[pos];
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_array_del(obj_id_t array_id, int pos, int do_ref_cnt)
{
  array_t *a = gfx_obj_array_ptr_rw(array_id);

  if(!a) return;

  if(pos < 0) pos = (int) a->size + pos;

  if(pos < 0 || pos >= (int) a->size) return;

  if(do_ref_cnt) {
    gfx_obj_ref_dec(a->ptr[pos]);
  }

  a->size--;

  if(a->size > (unsigned) pos) {
    gfx_memcpy(&a->ptr[pos], &a->ptr[pos + 1], (sizeof *a->ptr) * (a->size - (unsigned) pos));
  }

  a->ptr[a->size] = 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_push(obj_id_t array_id, obj_id_t id, int do_ref_cnt)
{
  array_t *a = gfx_obj_array_ptr(array_id);

  if(!a) return 0;

  return gfx_obj_array_set(array_id, id, (int) a->size, do_ref_cnt);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_pop(obj_id_t array_id, int do_ref_cnt)
{
  array_t *a = gfx_obj_array_ptr_rw(array_id);

  if(!a) return 0;

  if(a->size > 0) {
    a->size--;
    obj_id_t id = a->ptr[a->size];
    // do not leave invalid values behind
    a->ptr[a->size] = 0;
    if(do_ref_cnt) gfx_obj_ref_dec(id);
    return id;
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_array_pop_n(unsigned n, obj_id_t array_id, int do_ref_cnt)
{
  unsigned u;

  for(u = 0; u < n; u++) {
    gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_array_add(obj_id_t array_id, obj_id_t id, int do_ref_cnt)
{
  array_t *a = gfx_obj_array_ptr(array_id);

  if(!a) return 0;

  unsigned idx;

  for(idx = 0; idx < a->size; idx++) {
    if(a->ptr[idx] == id) return array_id;
  }

  return gfx_obj_array_set(array_id, id, (int) a->size, do_ref_cnt);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_array_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  array_t *array = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned idx, more_gc = 0;

  if(array && data_size == OBJ_ARRAY_SIZE(array->max)) {
    for(idx = 0; idx < array->size; idx++) {
      more_gc += gfx_obj_ref_dec_delay_gc(array->ptr[idx]);
    }
  }

  return more_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_array_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  array_t *array = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned idx;

  if(array && data_size == OBJ_ARRAY_SIZE(array->max)) {
    for(idx = 0; idx < array->size; idx++) {
      if(id == array->ptr[idx]) return 1;
    }
  }

  return 0;
}
