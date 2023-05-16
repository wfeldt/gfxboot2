#include <gfxboot/gfxboot.h>
#include <gfxboot/vocabulary.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// mem

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_mem_new(uint32_t size, uint8_t sub_type)
{
  obj_id_t id = gfx_obj_alloc(OTYPE_MEM, size);

  if(sub_type) {
    obj_t *ptr = gfx_obj_ptr(id);
    if(ptr) ptr->sub_type = sub_type;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
data_t *gfx_obj_mem_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_MEM) return 0;

  return &ptr->data;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
data_t *gfx_obj_mem_ptr_rw(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_MEM) return 0;

  if(ptr->flags.ro) {
   GFX_ERROR(err_readonly);
   return 0;
  }

  return &ptr->data;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
data_t *gfx_obj_mem_subtype_ptr(obj_id_t id, uint8_t subtype)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_MEM || ptr->sub_type != subtype) return 0;

  return &ptr->data;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_const_mem_nofree_new(const uint8_t *str, unsigned len, uint8_t sub_type, obj_id_t ref_id)
{
  obj_id_t id = 0;

  if(str) {
    // avoid recursive references, always point to original object
    if(ref_id) {
      obj_t *ptr;
      while((ptr = gfx_obj_ptr(ref_id))) {
        if(ptr->base_type != OTYPE_MEM || !ptr->data.ref_id) break;
        ref_id = ptr->data.ref_id;
      }

      if(ptr && ptr->base_type == OTYPE_MEM) ptr->flags.has_ref = 1;
    }

    id = gfx_obj_new(OTYPE_MEM);
    obj_t *optr = gfx_obj_ptr(id);
    if(optr) {
      optr->data.ptr = (uint8_t *) str;
      optr->data.size = len;
      optr->data.ref_id = ref_id;
      optr->flags.ro = 1;
      optr->flags.nofree = 1;
      optr->sub_type = sub_type;

      gfx_obj_ref_inc(ref_id);
    }
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_asciiz_new(const char *str)
{
  return gfx_obj_const_mem_nofree_new((uint8_t *) str, gfx_strlen(str), t_string, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_mem_dup(obj_id_t id, unsigned extra_bytes)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_MEM) return 0;

  obj_id_t new_id = gfx_obj_new(OTYPE_MEM);
  obj_t *new_ptr = gfx_obj_ptr(new_id);

  if(new_ptr) {
    data_t *data = OBJ_DATA_FROM_PTR(ptr);
    if((new_ptr->data.ptr = gfx_malloc(data->size + extra_bytes, new_id))) {
      new_ptr->data.size = data->size + extra_bytes;
      new_ptr->sub_type = ptr->sub_type;
      gfx_memcpy(new_ptr->data.ptr, data->ptr, data->size);
    }
    else {
      // delete, keep generation counter
      *new_ptr = (obj_t) { gen:new_ptr->gen };
      new_id = 0;
    }
  }

  return new_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// returns value (0..255) or -1 (invalid)
int gfx_obj_mem_get(obj_id_t mem_id, int pos)
{
  data_t *mem = gfx_obj_mem_ptr(mem_id);

  if(!mem) return -1;

  if(pos < 0) pos = (int) mem->size + pos;

  if(pos < 0 || pos >= (int) mem->size) return -1;

  return ((uint8_t *) mem->ptr)[pos];
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_mem_del(obj_id_t mem_id, int pos)
{
  data_t *mem = gfx_obj_mem_ptr_rw(mem_id);

  if(!mem) return;

  if(pos < 0) pos = (int) mem->size + pos;

  if(pos < 0 || pos >= (int) mem->size) return;

  mem->size--;

  if(mem->size > (unsigned) pos) {
    gfx_memcpy(mem->ptr + pos, mem->ptr + pos + 1, mem->size - (unsigned) pos);
  }

  ((uint8_t *) mem->ptr)[mem->size] = 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_mem_set(obj_id_t mem_id, uint8_t val, int pos)
{
  data_t *mem = gfx_obj_mem_ptr_rw(mem_id);

  if(pos < 0) pos = (int) mem->size + pos;

  if(pos < 0) return 0;

  if(pos >= (int) mem->size) {
    if(!gfx_obj_realloc(mem_id, (unsigned) pos + 1)) return 0;
    mem = gfx_obj_mem_ptr(mem_id);
  }

  ((uint8_t *) mem->ptr)[pos] = val;

  return mem_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_mem_insert(obj_id_t mem_id, uint8_t val, int pos)
{
  data_t *mem = gfx_obj_mem_ptr_rw(mem_id);

  if(pos < 0) pos = (int) mem->size + pos;

  if(pos < 0) return 0;

  if(pos >= (int) mem->size) {
    if(!gfx_obj_realloc(mem_id, (unsigned) pos + 1)) return 0;
  }
  else {
    if(!gfx_obj_realloc(mem_id, mem->size + 1)) return 0;
    mem = gfx_obj_mem_ptr(mem_id);
    gfx_memcpy(mem->ptr + pos + 1, mem->ptr + pos, mem->size - 1 - (unsigned) pos);
  }

  ((uint8_t *) mem->ptr)[pos] = val;

  return mem_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_mem_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2)
{
  uint8_t *p = ptr->data.ptr;
  unsigned len = ptr->data.size;

  if(!p) {
    GFX_ERROR(err_internal);
    return 0;
  }

  if(*idx >= len) {
    return 0;
  }

  *id1 = gfx_obj_num_new(p[*idx], t_int);
  (*idx)++;

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_mem_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  uint8_t *p = ptr->data.ptr;
  unsigned cnt, len = ptr->data.size;
  char s[17];
  int s_idx = 0;
  unsigned sub_type = ptr->sub_type;
  obj_id_t ref_id = ptr->data.ref_id;

  if(!p) return 1;

  if(!style.dump) {
    if(!style.ref) {
      if(style.inspect) {
        if(ref_id) {
          gfxboot_log("%s, ", gfx_obj_id2str(ref_id));
          data_t *ref_data = gfx_obj_mem_ptr(ref_id);
          if(ref_data && ptr->data.ptr >= ref_data->ptr) {
            gfxboot_log("ofs 0x%x, ", (unsigned) (ptr->data.ptr - ref_data->ptr));
          }
        }
        gfxboot_log("size %u", len);
        // if(sub_type) gfxboot_log(", subtype %u", sub_type);
      }

      if(sub_type == t_string || sub_type == t_word || sub_type == t_ref) {
        if(style.inspect) gfxboot_log(", ");
        gfxboot_log("\"");
        for(cnt = 0; cnt < len; cnt++) {
          gfxboot_log("%c", p[cnt] < 0x20 ? ' ' : p[cnt]);
        }
        gfxboot_log("\"");
      }
      else {
        return 0;
      }

      return 1;
    }
  }
  else {
    if(style.max && len > style.max) len = style.max;		// log at most this much
    gfxboot_log("   ");
    s_idx = 0;
    for(cnt = 0; cnt < len; cnt++) {
      gfxboot_log(" %02x", p[cnt]);
      s[s_idx++] = (p[cnt] >= 0x20 && p[cnt] < 0x7f) ? (char) p[cnt] : '.';
      if((cnt & 15) == 15) {
        s[s_idx] = 0;
        gfxboot_log("  %s", s);
        s_idx = 0;
      }
      if((cnt & 15) == 15 && cnt + 1 < len) {
        gfxboot_log("\n   ");
      }
    }
    if(s_idx) {
      s[s_idx] = 0;
      int spaces = 16 - s_idx;
      while(spaces--) gfxboot_log("   ");
      gfxboot_log("  %s", s);
    }
    gfxboot_log("\n");
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_mem_cmp(data_t *mem1, data_t *mem2)
{
  int result = 0;

  unsigned len = mem1->size < mem2->size ? mem1->size : mem2->size;

  if(len) {
    if((result = gfx_memcmp(mem1->ptr, mem2->ptr, len))) {
      result = result > 0 ? 1 : -1;
    }
  }

  if(!result && mem1->size != mem2->size) {
    result = mem1->size > mem2->size ? 1 : -1;
  }

  return result;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_mem_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  data_t *data = OBJ_DATA_FROM_PTR(ptr);

  return gfx_obj_ref_dec_delay_gc(data->ref_id);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_mem_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  data_t *data = OBJ_DATA_FROM_PTR(ptr);

  return data->ref_id == id;
}
