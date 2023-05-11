#include <gfxboot/gfxboot.h>
#define WITH_TYPE_NAMES 1
#include <gfxboot/vocabulary.h>

typedef struct {
  unsigned data_is_ptr:1;
  dump_function_t dump_function;
  gc_function_t gc_function;
  contains_function_t contains_function;
  iterate_function_t iterate_function;
} obj_descr_t;

static unsigned gfx_obj_data_is_ptr(unsigned type);
static dump_function_t gfx_obj_dump_function(unsigned type);
static gc_function_t gfx_obj_gc_function(unsigned type);
static iterate_function_t gfx_obj_iterate_function(unsigned type);
static int gfx_obj_none_dump(obj_t *ptr, dump_style_t style);
static int gfx_obj_invalid_dump(obj_t *ptr, dump_style_t style);
static unsigned gfx_obj_none_gc(obj_t *ptr);
static int gfx_obj_none_contains(obj_t *ptr, obj_id_t id);
static unsigned gfx_obj_none_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2);

obj_descr_t obj_descr[] = {
  [OTYPE_NONE] = {
    0,
    gfx_obj_none_dump,
    0,
    0,
    0
  },
  [OTYPE_MEM]     = {
    1,
    gfx_obj_mem_dump,
    gfx_obj_mem_gc,
    gfx_obj_mem_contains,
    gfx_obj_mem_iterate
  },
  [OTYPE_OLIST] = {
    1,
    gfx_obj_olist_dump,
    0,
    0,
    0
  },
  [OTYPE_FONT] = {
    1,
    gfx_obj_font_dump,
    gfx_obj_font_gc,
    gfx_obj_font_contains,
    0
  },
  [OTYPE_CANVAS] = {
    1,
    gfx_obj_canvas_dump,
    gfx_obj_canvas_gc,
    gfx_obj_canvas_contains,
    0
  },
  [OTYPE_ARRAY] = {
    1,
    gfx_obj_array_dump,
    gfx_obj_array_gc,
    gfx_obj_array_contains,
    gfx_obj_array_iterate
  },
  [OTYPE_HASH] = {
    1,
    gfx_obj_hash_dump,
    gfx_obj_hash_gc,
    gfx_obj_hash_contains,
    gfx_obj_hash_iterate
  },
  [OTYPE_CONTEXT] = {
    1,
    gfx_obj_context_dump,
    gfx_obj_context_gc,
    gfx_obj_context_contains,
    0
  },
  [OTYPE_NUM] = {
    0,
    gfx_obj_num_dump,
    0,
    0,
    0
  },
  [OTYPE_INVALID] = {
    0,
    gfx_obj_invalid_dump,
    0,
    0,
    0
  },
};


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// object functions
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_init()
{
  unsigned size = 0x2;

  gfxboot_log("gfx_obj_init(%u)\n", size);

  gfxboot_data->vm.olist.ptr = gfx_malloc(OBJ_OLIST_SIZE(size), OBJ_ID(0, 1));

  olist_t *ol = gfxboot_data->vm.olist.ptr;
  ol->max = size;

  // create object for object list
  obj_t *ptr = gfx_obj_ptr(gfxboot_data->vm.olist.id = gfx_obj_new(OTYPE_OLIST));

  if(ptr) {
    ptr->ref_cnt = -1u;		// can't be deleted
    ptr->data.ptr = gfxboot_data->vm.olist.ptr;
    ptr->data.size = OBJ_OLIST_SIZE(size);
  }

  return ptr ? 0 : 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
char *gfx_obj_id2str(obj_id_t id)
{
  // corresponds to OTYPE_* defines
  static const char *names[] = { "nil", "mem", "olist", "font", "canv", "array", "hash", "ctx", "num" };
  static char buf[64], buf2[32];
  const char *s, *sub_type = "", *ro = "", *sticky = "";
  unsigned idx = OBJ_ID2IDX(id);
  unsigned gen = OBJ_ID2GEN(id);

  obj_t *ptr = gfx_obj_ptr_nocheck(id);
  if(ptr && id) {
    if(ptr->flags.ro) ro = ".ro";
    if(ptr->flags.sticky) sticky = ".sticky";
    if(ptr->sub_type) {
      if(ptr->sub_type < sizeof type_name / sizeof *type_name) {
        sub_type = type_name[ptr->sub_type];
      }
      else {
        sub_type = "?";
      }
    }
    s = ptr->base_type < sizeof names / sizeof *names ? names[ptr->base_type] : "???";
    if(ptr->ref_cnt != -1u) {
      gfxboot_snprintf(buf2, sizeof buf2, ".%d", ptr->ref_cnt);
    }
    else {
      gfx_memcpy(buf2, ".*", sizeof ".*");
    }
  }
  else {
    s = id ? "?" : "nil";
    *buf2 = 0;
  }

  gfxboot_snprintf(buf, sizeof buf, "#%u.%u%s.%s%s%s%s%s", idx, gen, buf2, s, *sub_type ? "." : "", sub_type, ro, sticky);

  return buf;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_dump(obj_id_t id, dump_style_t style)
{
  unsigned type;
  obj_t *ptr;

  ptr = gfx_obj_ptr(id);

  if(!ptr) {
    if(style.dump) {
      gfxboot_log("= object dump (id %s): not found\n", gfx_obj_id2str(id));
    }
    else {
      if(style.inspect) {
        gfxboot_log("%s <%s>", gfx_obj_id2str(id), id ? "undef" : "nil");
      }
      else {
        gfxboot_log("%s", id ? "undef" : "nil");
      }
      if(!style.no_nl) gfxboot_log("\n");
    }

    return;
  }

  if(style.dump && !style.no_head) {
    gfxboot_log("== object dump (id %s) ==\n", gfx_obj_id2str(id));
  }

  type = ptr->base_type;

  if(type) {
    char *id_str = gfx_obj_id2str(id);
    if(style.dump) {
      gfxboot_log("  %s <", id_str);
      gfx_obj_dump_function(type)(ptr, (dump_style_t) { .inspect = 1, .max = style.max });
      if(gfxboot_data->vm.debug.show_pointer && ptr->flags.data_is_ptr) {
        gfxboot_log(", data %p[%u]", ptr->data.ptr, ptr->data.size);
      }
      gfxboot_log(">\n");
      gfx_obj_dump_function(type)(ptr, (dump_style_t) { .inspect = 1, .ref = 1, .dump = 1, .max = style.max });
    }
    else {
      if(style.ref) {
        gfx_obj_dump_function(type)(ptr, (dump_style_t) { .inspect = style.inspect, .ref = 1, .max = style.max });
      }
      else {
        if(style.inspect) {
          gfxboot_log("%s <", id_str);
          gfx_obj_dump_function(type)(ptr, (dump_style_t) { .inspect = 1, .max = style.max });
          gfxboot_log(">");
        }
        else {
          if(!gfx_obj_dump_function(type)(ptr, (dump_style_t) { .inspect = 0, .max = style.max })) {
            gfxboot_log("%s", id_str);
          }
        }
        if(!style.no_nl) gfxboot_log("\n");
      }
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// Note: id1 and id2 have their reference count already increased!
//
unsigned gfx_obj_iterate(obj_id_t id, unsigned *idx, obj_id_t *id1, obj_id_t *id2)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr) {
    *idx = 0;
    return 0;
  }

  return gfx_obj_iterate_function(ptr->base_type)(ptr, idx, id1, id2);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_new(unsigned type)
{
  uint32_t u, u2;
  olist_t *ol = gfxboot_data->vm.olist.ptr;
  unsigned size = ol->max;
  obj_t *ptr = ol->ptr;

  type &= OBJ_TYPE_MASK;

  if(type == OTYPE_NONE) return 0;

  //  ol->next = 0;

  for(u = 0, u2 = ol->next; u < size; u++, u2++) {
    if(u2 >= ol->max) u2 -= ol->max;
    if(ptr[u2].base_type == OTYPE_NONE) {
      ptr[u2].gen++;
      if(!ptr[u2].gen) ptr[u2].gen++;	// avoid generation count 0
      ptr[u2].base_type = type;
      ptr[u2].ref_cnt = 1;
      ptr[u2].flags.data_is_ptr = gfx_obj_data_is_ptr(type);

      ol->next = u2 + 1;
      if(ol->next >= ol->max) ol->next -= ol->max;

      return OBJ_ID(u2, ptr[u2].gen);
    }
  }

  // object list too small, realloc enlarged one
  size += (size >> 3) + 0x100;
  if(gfx_obj_realloc(gfxboot_data->vm.olist.id, OBJ_OLIST_SIZE(size))) {
    olist_t *ol2 = gfx_obj_olist_ptr(gfxboot_data->vm.olist.id);
    if(ol2) {
      ol2->max = size;
      return gfx_obj_new(type);
    }
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_alloc(unsigned type, uint32_t size)
{
  obj_id_t id = gfx_obj_new(type);
  obj_t *optr = gfx_obj_ptr(id);

  if(optr) {
    optr->data.size = size;
    optr->data.ptr = gfx_malloc(size, id);

    if(!optr->data.ptr) {
      // cleanup entry but keep generation counter
      *optr = (obj_t) { gen:optr->gen };
      id = 0;
    }
  }

  if(gfxboot_data->vm.debug.trace.memcheck && gfx_malloc_check(mc_basic + mc_xref)) {
    gfxboot_log("-- error in gfx_obj_alloc\n");
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_realloc(obj_id_t id, uint32_t size)
{
  obj_t *ptr = gfx_obj_ptr(id);
  void *ptr_old, *ptr_new;
  uint32_t size_old;

  if(ptr && ptr->flags.data_is_ptr) {
    ptr_new = gfx_malloc(size, id);

    if(ptr_new) {
      size_old = ptr->data.size;
      ptr_old = ptr->data.ptr;

      ptr->data.ptr = ptr_new;
      ptr->data.size = size;

      if(size_old > size) size_old = size;
      if(size_old && ptr_old) {
        gfx_memcpy(ptr_new, ptr_old, size_old);
      }

      // store link to new object list
      if(id == gfxboot_data->vm.olist.id) {
        gfxboot_data->vm.olist.ptr = ptr_new;
      }

      gfx_free(ptr_old);
    }
    else {
      id = 0;
    }
  }

  if(gfxboot_data->vm.debug.trace.memcheck && gfx_malloc_check(mc_basic + mc_xref)) {
    gfxboot_log("-- error in gfx_obj_realloc\n");
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_ref_inc(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(ptr) {
    if(gfxboot_data->vm.debug.trace.gc) gfxboot_log("GC: ++%s\n", gfx_obj_id2str(id));

    if(ptr->ref_cnt != -1u) ptr->ref_cnt++;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_ref_dec(obj_id_t id)
{
  if(gfx_obj_ref_dec_delay_gc(id)) gfx_obj_run_gc();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Decrement ref counter of id and add to gc list if ref count becomes 0.
//
// return:
//   1: entry to gc list added
//   0: nothing changed
//
unsigned gfx_obj_ref_dec_delay_gc(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr) return 0;

  if(gfxboot_data->vm.debug.trace.gc) gfxboot_log("GC: --%s\n", gfx_obj_id2str(id));

  if(ptr->ref_cnt && ptr->ref_cnt != -1u) {
    ptr->ref_cnt--;
  }

  if(ptr->ref_cnt == 0) {
    if(!gfxboot_data->vm.gc_list) {
      gfxboot_data->vm.gc_list = gfx_obj_array_new(0);
    }
    if(gfxboot_data->vm.gc_list) {
      gfx_obj_array_push(gfxboot_data->vm.gc_list, id, 0);

      return 1;
    }
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_run_gc()
{
  unsigned idx, type;
  obj_id_t id;
  obj_t *optr;
  void *data_ptr;
  array_t *a = gfx_obj_array_ptr(gfxboot_data->vm.gc_list);

  if(!a) return;

  for(idx = 0; idx < a->size; idx++) {
    id = a->ptr[idx];
    if(!id) continue;
    optr = gfx_obj_ptr(id);
    if(!optr) continue;
    type = optr->base_type;
    data_ptr = optr->data.ptr;

    unsigned more_gc = gfx_obj_gc_function(type)(optr);

    // ptr to gc_list might be outdated
    if(more_gc) {
      a = gfx_obj_array_ptr(gfxboot_data->vm.gc_list);
    }

    if(optr->flags.data_is_ptr) {
      if(!optr->flags.nofree) {
        gfx_free(data_ptr);
      }
    }

    // keep generation counter
    *optr = (obj_t) { gen:optr->gen };
  }

  // clear gc list
  a->size = 0;

  if(gfxboot_data->vm.debug.trace.memcheck && gfx_malloc_check(mc_basic + mc_xref)) {
    gfxboot_log("-- error in gfx_obj_run_gc\n");
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_t *gfx_obj_ptr_nocheck(obj_id_t id)
{
  uint32_t idx;
  olist_t *ol;

  idx = OBJ_ID2IDX(id);

  ol = gfxboot_data->vm.olist.ptr;

  if(!ol || idx >= ol->max) return 0;

  return ol->ptr + idx;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_t *gfx_obj_ptr(obj_id_t id)
{
  obj_t *ptr;

  if(!id) return 0;

  ptr = gfx_obj_ptr_nocheck(id);

  if(!ptr) return 0;

  if(ptr->base_type == OTYPE_NONE || ptr->gen != OBJ_ID2GEN(id)) {
    // nonexistent or outdated
    return 0;
  }

  return ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_data_is_ptr(unsigned type)
{
  if(type >= sizeof obj_descr/ sizeof *obj_descr) type = OTYPE_INVALID;

  return obj_descr[type].data_is_ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
dump_function_t gfx_obj_dump_function(unsigned type)
{
  if(type >= sizeof obj_descr/ sizeof *obj_descr) type = OTYPE_INVALID;

  return obj_descr[type].dump_function;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
gc_function_t gfx_obj_gc_function(unsigned type)
{
  if(type >= sizeof obj_descr/ sizeof *obj_descr) type = OTYPE_INVALID;

  return obj_descr[type].gc_function ?: gfx_obj_none_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
contains_function_t gfx_obj_contains_function(unsigned type)
{
  if(type >= sizeof obj_descr/ sizeof *obj_descr) type = OTYPE_INVALID;

  return obj_descr[type].contains_function ?: gfx_obj_none_contains;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
iterate_function_t gfx_obj_iterate_function(unsigned type)
{
  if(type >= sizeof obj_descr/ sizeof *obj_descr) type = OTYPE_INVALID;

  return obj_descr[type].iterate_function ?: gfx_obj_none_iterate;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_none_dump(obj_t *ptr, dump_style_t style)
{
  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_invalid_dump(obj_t *ptr, dump_style_t style)
{
  gfxboot_log("      <invalid type>\n");

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_none_gc(obj_t *ptr)
{
  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_none_contains(obj_t *ptr, obj_id_t id)
{

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_none_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2)
{
  return 0;
}
