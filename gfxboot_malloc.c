#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// malloc functions
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_init()
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  if(!head->ptr || head->size < 0x1000) return 1;

  head->first_chunk = head->ptr;

  *(malloc_chunk_t *) head->first_chunk = (malloc_chunk_t) { .next = head->size, .prev = 0, .id = 0 };

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_malloc_dump(dump_style_t style)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *p, *p_start, *p_end;
  malloc_chunk_t *chunk;
  unsigned idx;

  p_start = head->first_chunk;
  p_end = p_start + head->size;

  gfxboot_log("===  memory dump  ===\n");

  if(!p_start) {
    gfxboot_log("  no memory\n");

    return;
  }

  if(!style.no_check) {
    gfx_malloc_check();
    gfx_malloc_check_reverse();
  }

  for(idx = 0, p = p_start; p >= p_start && p < p_end; p += chunk->next, idx++) {
    if(style.max && idx >= style.max) break;
    chunk = (malloc_chunk_t *) p;
    gfxboot_log("%4u: ", idx);
    if(gfxboot_data->vm.debug.show_pointer) {
      gfxboot_log("%p", chunk->data);
    }
    else {
      gfxboot_log("0x%08x", (int) ((void *) (chunk->data) - p_start));
    }
    gfxboot_log("[%8d]", (int) (chunk->next - sizeof (malloc_chunk_t)));
    if(style.dump) gfxboot_log(" [%8d/%8d]", chunk->prev, chunk->next);
    if(chunk->id && (style.inspect || style.dump)) {
      gfxboot_log("  ");
      gfx_obj_dump(chunk->id, (dump_style_t) { .inspect = 1, .no_nl = 1 });
    }
    gfxboot_log("\n");
    if(chunk->next <= sizeof (malloc_chunk_t)) break;
  }

  if(!style.max && p != p_end) {
    gfxboot_log("  -- malloc chain corrupt --\n");
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void *gfx_malloc(uint32_t size, obj_id_t id)
{
  uint8_t *p, *p_start, *p_end;
  malloc_chunk_t *m, *m_next;
  void *mem = 0;

  if(id == 0) id = 1;	// ensure it's not 0

  p_start = gfxboot_data->vm.mem.ptr;
  p_end = p_start + gfxboot_data->vm.mem.size;

  if(size > gfxboot_data->vm.mem.size) return mem;

  // size 0: return a valid pointer that we won't try to free
  if(size == 0) return p_end;

  size += sizeof (malloc_chunk_t);	// include header size
  size = (size + 3) & ~3U;		// align a bit

  for(p = p_start; p >= p_start && p < p_end; p += m->next) {
    m = (malloc_chunk_t *) p;
    if(m->id == 0 && m->next >= size) {
      m->id = id;
      mem = p + sizeof (malloc_chunk_t);
      gfx_memset(mem, 0, size - sizeof (malloc_chunk_t));
      if(m->next > size + sizeof (malloc_chunk_t)) {
        m_next = (malloc_chunk_t *) (p + m->next);
        uint32_t n = m->next - size;
        m->next = size;
        m = (malloc_chunk_t *) (p + size);
        m->id = 0;
        m->next = n;
        m->prev = size;
        m_next->prev = n;
      }
      break;
    }
  }

  return mem;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_free(void *ptr)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *p_start = head->first_chunk;
  void *p_end = p_start + head->size;

  // point to chunk header
  void *mem = ptr - sizeof (malloc_chunk_t);

  if(!mem || mem < p_start || mem >= p_end) return;

  malloc_chunk_t *chunk = mem;

  uint32_t prev = chunk->prev;
  uint32_t next = chunk->next;

  void *mem_prev = mem - prev;
  void *mem_next = mem + next;

  malloc_chunk_t *chunk_prev = mem_prev;
  malloc_chunk_t *chunk_next = mem_next;

  if(prev) {
    if(mem_prev < p_start || mem_prev >= mem || chunk_prev->next != prev) goto error;

    // join with preceeding free chunk
    if(chunk_prev->id == 0) {
      chunk_prev->next += next;
      mem = mem_prev;
      chunk = mem;
      next = chunk->next;
      if(mem_next != p_end) {
        chunk_next->prev = next;
      }
    }
  }

  if(mem_next <= mem || mem_next > p_end || chunk_next->prev != next) goto error;

  if(mem_next != p_end) {
    // join with following free chunk
    if(chunk_next->id == 0) {
      chunk->next += chunk_next->next;
      mem_next = mem + chunk->next;
      chunk_next = mem_next;
      if(mem_next != p_end) {
        chunk_next->prev = chunk->next;
      }
    }
  }

  chunk->id = 0;

  goto end;

error:

  GFX_ERROR(err_memory_corruption);

end:

  if(gfxboot_data->vm.debug.trace.memcheck && gfx_malloc_check()) {
    gfxboot_log("-- error in gfx_free\n");
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_check()
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *p, *p_start, *p_prev, *p_next, *p_end;
  malloc_chunk_t *chunk, *chunk_prev, *chunk_next;
  unsigned idx;

  p_start = head->first_chunk;
  p_end = p_start + head->size;

  if(!p_start) return 0;

  for(idx = 0, p = p_prev = p_start; p >= p_start && p < p_end; p_prev = p, p = p_next, idx++) {
    chunk = (malloc_chunk_t *) p;
    chunk_prev = (malloc_chunk_t *) p_prev;

    p_next = p + chunk->next;
    if(chunk->next < sizeof (malloc_chunk_t) || p_next > p_end || p_next <= p) break;

    chunk_next = (malloc_chunk_t *) p_next;

    if(chunk != chunk_prev && chunk->prev != chunk_prev->next) break;

    if(p_next != p_end && chunk->next != chunk_next->prev) break;

    if(chunk->id && gfxboot_data->vm.olist.ptr) {
      obj_t *obj_ptr = gfx_obj_ptr(chunk->id);
      if(!obj_ptr) goto error;
      if(!obj_ptr->flags.data_is_ptr) goto error;
      if(obj_ptr->flags.nofree) continue;
      void *d_start = obj_ptr->data.ptr;
      void *d_end = d_start + obj_ptr->data.size;
      if(
        d_start < p + sizeof (malloc_chunk_t) ||
        d_start > p_next ||
        d_end < p + sizeof (malloc_chunk_t) ||
        d_end > p_next
      ) {
        gfxboot_log("-- referenced object #%u not inside malloc chunk\n", OBJ_ID2IDX(chunk->id));
        goto error;
      }
    }
  }

  if(p != p_end) goto error;

  return 0;

error:
  gfxboot_log("-- malloc chain corrupt (entry %d)\n", idx);

  GFX_ERROR(err_memory_corruption);

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_check_reverse()
{
  malloc_head_t *head = &gfxboot_data->vm.mem;
  olist_t *obj_list = gfxboot_data->vm.olist.ptr;
  unsigned obj_idx;

  if(!obj_list) return 0;

  void *p_start = head->first_chunk;
  void *p_end = p_start + head->size;

  for(obj_idx = 0; obj_idx < obj_list->max; obj_idx++) {
    obj_t *obj_ptr = obj_list->ptr + obj_idx;
    if(obj_ptr->base_type == OTYPE_NONE) continue;
    if(!obj_ptr->flags.data_is_ptr || obj_ptr->flags.nofree) continue;
    // special case: size 0 objects point to end of malloc area
    if(obj_ptr->data.size == 0 && obj_ptr->data.ptr == p_end) continue;
    malloc_chunk_t *chunk = gfx_malloc_find_chunk(obj_ptr->data.ptr);
    if(!chunk) goto error;
    void *chunk_ptr = chunk;
    void *d_end = obj_ptr->data.ptr + obj_ptr->data.size;
    if(
      !chunk->id ||
      d_end < chunk_ptr + sizeof (malloc_chunk_t) ||
      d_end > chunk_ptr + chunk->next
    ) {
      gfxboot_log("-- referenced object #%u not inside its malloc chunk\n", obj_idx);
      goto error;
    }

    obj_id_t id = obj_ptr->data.ref_id;
    if(!id) id = OBJ_ID(obj_idx, obj_ptr->gen);

    if(chunk->id != id) {
      gfxboot_log(
        "-- malloc chunk id mismatch (malloc id #%u, obj id #%u)\n",
        OBJ_ID2IDX(chunk->id), OBJ_ID2IDX(id)
      );
    }
  }

  return 0;

error:
  gfxboot_log("-- object store corrupt (id #%u)\n", obj_idx);

  GFX_ERROR(err_memory_corruption);

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
malloc_chunk_t *gfx_malloc_find_chunk(void *addr)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *p, *p_start, *p_next, *p_end;
  malloc_chunk_t *chunk;

  p_start = head->first_chunk;
  p_end = p_start + head->size;

  if(addr < p_start || addr >= p_end) return 0;

  for(p = p_start; p < p_end; p = p_next) {
    chunk = (malloc_chunk_t *) p;
    p_next = p + chunk->next;
    if(addr >= p && addr < p_next) {
      return (unsigned) (addr - p) < sizeof (malloc_chunk_t) ? 0 : p;
    }
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_defrag(unsigned max)
{


}
