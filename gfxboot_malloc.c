#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// malloc functions
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_init()
{
  malloc_header_t *m = gfxboot_data->vm.mem.ptr;

  if(!m || gfxboot_data->vm.mem.size < 0x1000) return 1;

  m->next = gfxboot_data->vm.mem.size;
  m->id = 0;

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_malloc_dump(dump_style_t style)
{
  uint8_t *p, *p_start, *p_end;
  malloc_header_t *m;
  unsigned idx, x, g;

  p_start = gfxboot_data->vm.mem.ptr;
  p_end = p_start + gfxboot_data->vm.mem.size;

  gfxboot_log("===  memory dump  ===\n");

  if(!p_start) {
    gfxboot_log("  no memory\n");

    return;
  }

  for(idx = 0, p = p_start; p >= p_start && p < p_end; p += m->next, idx++) {
    if(style.max && idx >= style.max) break;
    m = (malloc_header_t *) p;
    x = OBJ_ID2IDX(m->id);
    g = OBJ_ID2GEN(m->id);
    gfxboot_log("%4u: %4u.%02x, ", idx, x, g);
    if(gfxboot_data->vm.debug.show_pointer) {
      gfxboot_log("%p", m + 1);
    }
    else {
      gfxboot_log("0x%08x", (int) (p - p_start) + (int) sizeof (malloc_header_t));
    }
    gfxboot_log("[%8d]\n", (int) (m->next - sizeof (malloc_header_t)));
    if(m->next <= sizeof (malloc_header_t)) break;
  }

  if(!style.max && p != p_end) {
    gfxboot_log("  -- malloc chain corrupt --\n");
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void *gfx_malloc(uint32_t size, uint32_t id)
{
  uint8_t *p, *p_start, *p_end;
  malloc_header_t *m;
  void *mem = 0;

  if(id == 0) id = 1;	// ensure it's not 0

  p_start = gfxboot_data->vm.mem.ptr;
  p_end = p_start + gfxboot_data->vm.mem.size;

  if(size > gfxboot_data->vm.mem.size) return mem;

  // size 0: return a valid pointer that we won't try to free
  if(size == 0) return p_end;

  size += sizeof (malloc_header_t);	// include header size
  size = (size + 3) & ~3U;		// align a bit

  for(p = p_start; p >= p_start && p < p_end; p += m->next) {
    m = (malloc_header_t *) p;
    if(m->id == 0 && m->next >= size) {
      m->id = id;
      mem = p + sizeof (malloc_header_t);
      gfx_memset(mem, 0, size - sizeof (malloc_header_t));
      if(m->next > size + sizeof (malloc_header_t)) {
        uint32_t n = m->next - size;
        m->next = size;
        m = (malloc_header_t *) (p + size);
        m->id = 0;
        m->next = n;
      }
      break;
    }
  }

  return mem;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_free(void *ptr)
{
  uint8_t *p, *p_start, *p_end, *p_last, *mem = ptr;
  malloc_header_t *m, *m_last, *m_next;

  p_start = gfxboot_data->vm.mem.ptr;
  p_end = p_start + gfxboot_data->vm.mem.size;

  if(!mem || mem < p_start || mem >= p_end) return;

  // p < mem instead of 'p < p_end' for an early exit
  for(p = p_last = p_start; p >= p_start && p < mem; p_last = p, p += m->next) {
    m = (malloc_header_t *) p;

    if(mem == p + sizeof (malloc_header_t)) {
      if(m->id == 0) return;		// already free

      m->id = 0;			// mark as free

      if(p + m->next < p_end) {		// not last block
        m_next = (malloc_header_t *) (p + m->next);
        if(m_next->id == 0) {		// join next + current block
          m->next += m_next->next;
        }
      }

      if(p_last != p) {			// not first block
        m_last = (malloc_header_t *) p_last;
        if(m_last->id == 0) {		// join last + current block
          m_last->next += m->next;
        }
      }
    }
  }
}


