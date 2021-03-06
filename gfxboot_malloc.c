#include <gfxboot/gfxboot.h>


static int gfx_malloc_check_basic(void);
static int gfx_malloc_check_xref(void);

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
  head->first_free = head->ptr;

  *(malloc_chunk_t *) head->first_chunk = (malloc_chunk_t) { .prev = head->size, .next = head->size, .id = 0 };

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_malloc_dump(dump_style_t style)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  malloc_chunk_t *chunk;
  unsigned idx;
  void *mem;

  void *mem_start = head->first_chunk;
  void *mem_end = mem_start + head->size;

  gfxboot_log("===  memory dump  ===\n");

  if(!mem_start) {
    gfxboot_log("  no memory\n");

    return;
  }

  if(!style.no_check) gfx_malloc_check(mc_basic + mc_xref);

  for(idx = 0, mem = mem_start; mem >= mem_start && mem < mem_end; mem += chunk->next, idx++) {
    if(style.max && idx >= style.max) break;
    chunk = (malloc_chunk_t *) mem;
    gfxboot_log("%4u%c ", idx, mem == head->first_free ? '*' : ':');
    if(gfxboot_data->vm.debug.show_pointer) {
      gfxboot_log("%p", chunk->data);
    }
    else {
      gfxboot_log("0x%08x", (int) ((void *) (chunk->data) - mem_start));
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

  if(!style.max && mem != mem_end) {
    gfxboot_log("  -- malloc chain corrupt --\n");
 }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void *gfx_malloc(uint32_t size, obj_id_t id)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *mem_start = head->first_chunk;
  void *mem_end = mem_start + head->size;

  malloc_chunk_t *chunk;

  // can never be 0
  if(id == 0) return 0;

  // out of memory
  if(size > gfxboot_data->vm.mem.size) return 0;

  // size 0: return a valid pointer that we won't try to free
  if(size == 0) return mem_end;

  size += sizeof (malloc_chunk_t);	// include header size
  size = (size + 3) & ~3U;		// align to 4 byte

  unsigned first_free_seen = 0;

  for(void *mem = head->first_free; mem >= mem_start && mem < mem_end; mem += chunk->next) {
    chunk = (malloc_chunk_t *) mem;
    if(chunk->id == 0) {
      if(!first_free_seen) head->first_free = mem;
      first_free_seen++;

      if(chunk->next >= size) {
        chunk->id = id;
        gfx_memset(mem + sizeof (malloc_chunk_t), 0, size - sizeof (malloc_chunk_t));
        // big enough to split off an empty chunk
        if(chunk->next > size + sizeof (malloc_chunk_t)) {
          void *mem_next = mem + chunk->next;
          malloc_chunk_t *chunk_next = mem_next == mem_end ? mem_start : mem_next;
          uint32_t n = chunk->next - size;
          chunk->next = size;
          chunk = (malloc_chunk_t *) (mem + size);
          if(first_free_seen == 1) head->first_free = mem + size;
          chunk->id = 0;
          chunk->next = n;
          chunk->prev = size;
          chunk_next->prev = n;
        }

        return mem + sizeof (malloc_chunk_t);
      }
    }
  }

  // out of memory
  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_free(void *ptr)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *mem_start = head->first_chunk;
  void *mem_end = mem_start + head->size;

  if(!ptr || ptr < mem_start + sizeof (malloc_chunk_t) || ptr >= mem_end) return;

  // point to chunk header
  void *mem = ptr - sizeof (malloc_chunk_t);

  malloc_chunk_t *chunk = mem;

  uint32_t next = chunk->next;
  void *mem_next = mem != mem_end ? mem + next : mem_start;
  malloc_chunk_t *chunk_next = mem_next;

  if(mem != mem_start) {
    uint32_t prev = chunk->prev;
    void *mem_prev = mem - prev;
    malloc_chunk_t *chunk_prev = mem_prev;

    if(mem_prev < mem_start || mem_prev >= mem || chunk_prev->next != prev) goto error;

    // join with preceeding free chunk
    if(chunk_prev->id == 0) {
      chunk_prev->next += next;
      mem = mem_prev;
      chunk = mem;
      next = chunk->next;
      chunk_next->prev = next;
    }
  }

  if(mem_next <= mem || mem_next > mem_end || chunk_next->prev != next) goto error;

  if(mem_next != mem_end) {
    // join with following free chunk
    if(chunk_next->id == 0) {
      chunk->next += chunk_next->next;
      mem_next = mem + chunk->next != mem_end ? mem + chunk->next : mem_start;
      chunk_next = mem_next;
      chunk_next->prev = chunk->next;
    }
  }

  chunk->id = 0;

  if(mem < head->first_free) head->first_free = mem;

  if(gfxboot_data->vm.debug.trace.memcheck && gfx_malloc_check(mc_basic)) {
    gfxboot_log("-- error in gfx_free\n");
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }

  return;

error:
  gfxboot_log("-- free(0x%x) failed\n", (int) (ptr - mem_start));
  GFX_ERROR(err_memory_corruption);

  if(gfxboot_data->vm.debug.trace.memcheck) {
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_check_basic()
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *mem, *mem_start, *mem_prev, *mem_next, *mem_end;
  malloc_chunk_t *chunk, *chunk_start, *chunk_prev, *chunk_next;
  unsigned idx;

  mem_start = head->first_chunk;
  mem_end = mem_start + head->size;
  chunk_start = mem_start;

  if(!mem_start) return 0;

  unsigned free_chunk = 0, first_free_ok = 0;


  for(idx = 0, mem = mem_prev = mem_start; mem >= mem_start && mem < mem_end; mem_prev = mem, mem = mem_next, idx++) {
    chunk = (malloc_chunk_t *) mem;
    chunk_prev = (malloc_chunk_t *) mem_prev;

    if(mem == head->first_free && !free_chunk) first_free_ok = 1;

    if(chunk->id == 0) free_chunk = 1;

    mem_next = mem + chunk->next;
    if(chunk->next < sizeof (malloc_chunk_t) || mem_next > mem_end || mem_next <= mem) break;

    chunk_next = (malloc_chunk_t *) mem_next;

    if(chunk != chunk_prev && chunk->prev != chunk_prev->next) break;

    if(mem_next != mem_end && chunk->next != chunk_next->prev) break;

    if(chunk->id && gfxboot_data->vm.olist.ptr) {
      obj_t *obj_ptr = gfx_obj_ptr(chunk->id);
      if(!obj_ptr) goto error;
      if(!obj_ptr->flags.data_is_ptr) goto error;
      if(obj_ptr->flags.nofree) continue;
      void *data_start = obj_ptr->data.ptr;
      void *data_end = data_start + obj_ptr->data.size;
      if(
        data_start < mem + sizeof (malloc_chunk_t) ||
        data_start > mem_next ||
        data_end < mem + sizeof (malloc_chunk_t) ||
        data_end > mem_next
      ) {
        gfxboot_log("-- object #%u not inside malloc chunk\n", OBJ_ID2IDX(chunk->id));
        goto error;
      }
    }
  }

  if(!first_free_ok) {
    gfxboot_log("-- first free ptr wrong\n");
    goto error;
  }

  if(mem != mem_end) goto error;

  if(chunk->next != chunk_start->prev) goto error;

  return 0;

error:
  gfxboot_log("-- malloc chain corrupt (entry %d)\n", idx);

  GFX_ERROR(err_memory_corruption);

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_check_xref()
{
  malloc_head_t *head = &gfxboot_data->vm.mem;
  olist_t *obj_list = gfxboot_data->vm.olist.ptr;

  int err = 0;

  unsigned obj_idx;

  if(!obj_list) return 0;

  void *mem_end = head->first_chunk + head->size;

  for(obj_idx = 0; obj_idx < obj_list->max; obj_idx++) {
    obj_t *obj_ptr = obj_list->ptr + obj_idx;
    if(obj_ptr->base_type == OTYPE_NONE) continue;
    if(!obj_ptr->flags.data_is_ptr) continue;
    if(obj_ptr->flags.nofree) {
      obj_id_t ref_id = obj_ptr->data.ref_id;
      if(ref_id) {
        obj_t *ptr = gfx_obj_ptr(ref_id);
        if(!ptr || !ptr->flags.has_ref) {
          gfxboot_log("-- object #%u does not know it is referenced by #%u\n", OBJ_ID2IDX(ref_id), obj_idx);
          err = 1;
        }
      }
      continue;
    }
    // special case: size 0 objects point to end of malloc area
    if(obj_ptr->data.size == 0 && obj_ptr->data.ptr == mem_end) continue;
    malloc_chunk_t *chunk = gfx_malloc_find_chunk(obj_ptr->data.ptr);
    if(!chunk) {
      gfxboot_log("-- object #%u is missing its malloc chunk\n", obj_idx);
      err = 1;
      continue;
    }
    void *chunk_ptr = chunk;
    void *d_end = obj_ptr->data.ptr + obj_ptr->data.size;
    if(
      !chunk->id ||
      d_end < chunk_ptr + sizeof (malloc_chunk_t) ||
      d_end > chunk_ptr + chunk->next
    ) {
      gfxboot_log("-- object #%u not inside its malloc chunk\n", obj_idx);
      err = 1;
      continue;
    }

    obj_id_t id = obj_ptr->data.ref_id;
    if(!id) id = OBJ_ID(obj_idx, obj_ptr->gen);

    if(chunk->id != id) {
      gfxboot_log(
        "-- malloc chunk id mismatch (malloc id #%u, obj id #%u)\n",
        OBJ_ID2IDX(chunk->id), OBJ_ID2IDX(id)
      );
      err = 1;
    }
  }

  if(err) {
    gfxboot_log("-- object store corrupt\n");
    GFX_ERROR(err_memory_corruption);
  }

  return err;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_check(malloc_check_t what)
{
  int result = 0;

  if((what & mc_basic)) result |= gfx_malloc_check_basic();
  if((what & mc_xref)) result |= gfx_malloc_check_xref();

  return result;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
malloc_chunk_t *gfx_malloc_find_chunk(void *ptr)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *mem_start, *mem_next, *mem_end;
  malloc_chunk_t *chunk;

  mem_start = head->first_chunk;
  mem_end = mem_start + head->size;

  if(ptr < mem_start || ptr >= mem_end) return 0;

  for(void *mem = mem_start; mem < mem_end; mem = mem_next) {
    chunk = (malloc_chunk_t *) mem;
    mem_next = mem + chunk->next;
    if(ptr >= mem && ptr < mem_next) {
      return (unsigned) (ptr - mem) < sizeof (malloc_chunk_t) ? 0 : mem;
    }
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_defrag(unsigned max)
{
  malloc_head_t *head = &gfxboot_data->vm.mem;

  void *mem_start = head->first_chunk;
  void *mem_end = head->first_chunk + head->size;

  void *mem_defrag_start = 0;
  void *mem_defrag_end = 0;

  malloc_chunk_t *chunk, *chunk_prev, *chunk_next;

  obj_id_t ref_id = 0;
  int err = 0;
  unsigned cnt = 0;
  uint32_t diff = 0;
  int olist_adjusted = 0;

  for(void *mem = head->first_free; mem >= mem_start && mem < mem_end; mem += chunk->next) {
    chunk = (malloc_chunk_t *) mem;

    if(mem_defrag_start) cnt++;

    if(chunk->id == 0) {
      if(mem_defrag_start) {
        mem_defrag_end = mem;
        break;
      }
      else {
        mem_defrag_start = mem;
        diff = chunk->next;
      }
      continue;
    }

    if(!mem_defrag_start) continue;

    obj_t *ptr = gfx_obj_ptr(chunk->id);
    if(!ptr) {
      err = 1;
      break;
    }

    // is olist?
    if(chunk->id == OBJ_ID(0, 1)) olist_adjusted = 1;

    if(ptr->flags.data_is_ptr && !ptr->flags.nofree) {
      ptr->data.ptr -= diff;
    }

    if(ptr->flags.has_ref) {
      if(ref_id) {
        mem_defrag_end = mem;
        break;
      }
      ref_id = chunk->id;
    }

    if(cnt >= max) {
      mem_defrag_end = mem;
      break;
    }
  }

  // if olist was adjusted, adjust also global pointer
  if(olist_adjusted) {
    gfxboot_data->vm.olist.ptr -= diff;
  }

  if(!err && cnt && mem_defrag_start) {
    if(!mem_defrag_end) mem_defrag_end = mem_end;
    gfxboot_log(
      "-- defrag: %u chunks, diff %u, 0x%x - 0x%x, ref_id #%u\n",
      cnt,
      diff,
      (unsigned) (mem_defrag_start - mem_start),
      (unsigned) (mem_defrag_end - mem_start),
      OBJ_ID2IDX(ref_id)
    );

    chunk = mem_defrag_start;
    chunk_next = mem_defrag_start + diff;

    // adjust chunks at beginning
    chunk_prev = (mem_defrag_start != mem_start ? mem_defrag_start : mem_end) - chunk->prev;
    chunk_next->prev = chunk_prev->next;

    // remember last chunk size
    malloc_chunk_t *chunk_end = mem_defrag_end != mem_end ? mem_defrag_end : mem_start;
    uint32_t prev = chunk_end->prev;

    mem_defrag_end -= diff;

    // adjust pointer to first free block
    head->first_free = mem_defrag_end;

    // copy
    gfx_memcpy(mem_defrag_start, mem_defrag_start + diff, (unsigned) (mem_defrag_end - mem_defrag_start));

    // adjust chunks at end
    chunk = mem_defrag_end;
    chunk->id = 0;
    chunk->next = diff;
    chunk->prev = prev;
    chunk_end->prev = diff;

    void *mem_next = mem_defrag_end + diff;
    chunk_next = mem_next;

    // join free end blocks
    if(mem_next != mem_end && chunk_next->id == 0) {
      chunk->next += chunk_next->next;
      mem_next += chunk_next->next;
      if(mem_next == mem_end) mem_next = mem_start;
      chunk_next = mem_next;
      chunk_next->prev = chunk->next;
    }
  }

  // adjust everything pointing to ref_id
  if(ref_id) {
    olist_t *ol = gfxboot_data->vm.olist.ptr;

    for(unsigned u = 0; u < ol->max; u++) {
      obj_t *ptr = ol->ptr + u;
      if(
        ptr->base_type != OTYPE_NONE &&
        ptr->flags.data_is_ptr &&
        ptr->data.ref_id == ref_id
      ) {
        ptr->data.ptr -= diff;
      }
    }
  }

  if(gfxboot_data->vm.debug.trace.memcheck && gfx_malloc_check(mc_basic + mc_xref)) {
    GFX_ERROR(err_memory_corruption);
    gfxboot_log("-- error in gfx_defrag\n");
    gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1 });
  }
}
