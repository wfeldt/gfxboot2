#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// lib functions
//


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_show_error(void)
{
  if(gfxboot_data->vm.error.id && !gfxboot_data->vm.error.shown) {
#ifdef FULL_ERROR
    gfxboot_log(
      "error %d (%s), ip = %s, src = %s:%d\n",
      gfxboot_data->vm.error.id,
      gfx_error_msg(gfxboot_data->vm.error.id),
      gfx_debug_get_ip(),
      gfxboot_data->vm.error.src_file,
      gfxboot_data->vm.error.src_line
    );
#else
    gfxboot_log(
      "error %d (%s), ip = %s\n",
      gfxboot_data->vm.error.id,
      gfx_error_msg(gfxboot_data->vm.error.id),
      gfx_debug_get_ip()
    );
#endif
   gfxboot_data->vm.error.shown = 1;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char *gfx_error_msg(error_id_t id)
{
  static const char *error_names[] = {
    [err_ok] = "ok",
    [err_invalid_code] = "invalid code",
    [err_invalid_instruction] = "invalid instruction",
    [err_no_array_start] = "no array start",
    [err_no_hash_start] = "no hash start",
    [err_no_memory] = "no memory",
    [err_invalid_hash_key] = "invalid hash key",
    [err_stack_underflow] = "stack underflow",
    [err_internal] = "internal",
    [err_no_loop_context] = "no loop context",
    [err_invalid_range] = "invalid range",
    [err_invalid_data] = "invalid data",
    [err_readonly] = "readonly",
    [err_invalid_arguments] = "invalid arguments",
    [err_div_by_zero] = "div by zero",
    [err_memory_corruption] = "memory corruption",
  };

  if(id < sizeof error_names/sizeof *error_names) {
    return error_names[id];
  }
  else {
    return "weird error";
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint32_t gfx_read_le32(const void *p)
{
  const uint8_t *s = p;

  return (uint32_t) s[0] + ((uint32_t) s[1] << 8) + ((uint32_t) s[2] << 16) + ((uint32_t) s[3] << 24);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
char *gfx_utf8_enc(unsigned uc)
{
  static char buf[7];
  char *s = buf;

  uc &= 0x7fffffff;

  if(uc < 0x80) {			// 7 bits
    *s++ = uc;
  }
  else {
    if(uc < (1 << 11)) {		// 11 (5 + 6) bits
      *s++ = 0xc0 + (uc >> 6);
      goto utf8_encode_2;
    }
    else if(uc < (1 << 16)) {		// 16 (4 + 6 + 6) bits
      *s++ = 0xe0 + (uc >> 12);
      goto utf8_encode_3;
    }
    else if(uc < (1 << 21)) {		// 21 (3 + 6 + 6 + 6) bits
      *s++ = 0xf0 + (uc >> 18);
      goto utf8_encode_4;
    }
    else if(uc < (1 << 26)) {		// 26 (2 + 6 + 6 + 6 + 6) bits
      *s++ = 0xf8 + (uc >> 24);
      goto utf8_encode_5;
    }
    else {				// 31 (1 + 6 + 6 + 6 + 6 + 6) bits
      *s++ = 0xfc + (uc >> 30);
    }

    *s++ = 0x80 + ((uc >> 24) & ((1 << 6) - 1));

    utf8_encode_5:
      *s++ = 0x80 + ((uc >> 18) & ((1 << 6) - 1));

    utf8_encode_4:
      *s++ = 0x80 + ((uc >> 12) & ((1 << 6) - 1));

    utf8_encode_3:
      *s++ = 0x80 + ((uc >> 6) & ((1 << 6) - 1));

    utf8_encode_2:
      *s++ = 0x80 + (uc & ((1 << 6) - 1));
  }

  *s = 0;

  return buf;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Decode utf8 sequence.
//
// a) if s points to a valid utf8 sequence:
//  - returns unicode char (a non-negative number)
//  - s is updated to point past utf8 char
//
// b) if s does not point to a valid utf8 sequence
//  - returns negated first byte
//  - s is incremented by 1
//
int gfx_utf8_dec(char **s, unsigned *len)
{
  unsigned char *p;
  int c;
  unsigned u, l, l0;

  if(!s || (!*s && !*len)) return 0;

  p = (uint8_t *) *s;

  u = *p++;
  if(len) (*len)--;

  if(u >= 0x80) {
    if(u < 0xc0 || u >= 0xfe) {
      *s = (char *) p;
      return -(int) u;
    }
    l = 1;
    if(u < 0xe0) {
      c = u & 0x1f;
    }
    else if(u < 0xf0) {
      c = u & 0x0f;
      l = 2;
    }
    else if(u < 0xf8) {
      c = u & 0x07;
      l = 3;
    }
    else if(u < 0xfc) {
      c = u & 0x03;
      l = 4;
    }
    else if(u < 0xfe) {
      c = u & 0x01;
      l = 5;
    }
    if(len && l > *len) {
      *s = (char *) p;
      return -(int) u;
    }
    l0 = l;
    while(l--) {
      u = *p++;
      if(u < 0x80 || u >= 0xc0) {
        u = (uint8_t) **s;
        (*s)++;
        if(len) (*len)--;
        return -(int) u;
      }
      c = (c << 6) + ((int) u & 0x3f);
    }
    if(len) *len -= l0;
  }
  else {
    c = (int) u;
  }

  *s = (char *) p;

  return c;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_read_file(char *name)
{
  obj_id_t id = 0;
  void *buf = 0;

  int size = gfxboot_sys_read_file(name, &buf);

  if(size >= 0 && buf) {
    id = gfx_obj_mem_new((unsigned) size, 0);

    if(size) {
      data_t *mem = gfx_obj_mem_ptr(id);

      if(mem) gfx_memcpy(mem->ptr, buf, (unsigned) size);

      gfxboot_sys_free(buf);
    }
  }

  gfxboot_log("read(%s): id = #%08x\n", name, id);

  return id;
}


#if INCLUDE_DIV64
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static uint64_t __udivmoddi4(uint64_t num, uint64_t den, int mod) __attribute__((noinline));
uint64_t __udivdi3 (uint64_t a, uint64_t b);
uint64_t __umoddi3 (uint64_t a, uint64_t b);
int64_t __divdi3 (int64_t a, int64_t b);
int64_t __moddi3 (int64_t a, int64_t b);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint64_t __udivmoddi4(uint64_t num, uint64_t den, int mod)
{
  if(num < (1llu << 32) && den < (1llu << 32)) {
    return mod ? ((uint32_t) num % (uint32_t) den) : ((uint32_t) num / (uint32_t) den);
  }

  uint64_t bit = 1, res = 0;

  while(den < num && bit && (den & (1llu << 63)) == 0) {
    den <<= 1;
    bit <<= 1;
  }

  while(bit) {
    if(num >= den) {
      num -= den;
      res |= bit;
    }
    bit >>= 1;
    den >>= 1;
  }

  return mod ? num : res;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint64_t __udivdi3(uint64_t a, uint64_t b)
{
  return __udivmoddi4(a, b, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint64_t __umoddi3 (uint64_t a, uint64_t b)
{
  return __udivmoddi4(a, b, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int64_t __divdi3(int64_t a, int64_t b)
{
  unsigned sign = 0;

  if(a < 0) {
    a = -a;
    sign = 1;
  }

  if(b < 0) {
    b = -b;
    sign ^= 1;
  }

  int64_t r = (int64_t) __udivmoddi4((uint64_t) a, (uint64_t) b, 0);

  return sign ? -r : r;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int64_t __moddi3(int64_t a, int64_t b)
{
  int64_t r;

  if(b < 0) b = -b;

  if(a < 0) {
    r = - (int64_t) __udivmoddi4((uint64_t) (-a), (uint64_t) b, 1);
  }
  else  {
    r = (int64_t) __udivmoddi4((uint64_t) a, (uint64_t) b, 1);
  }

  return r;
}
#endif
