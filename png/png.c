// Implementation of a png decoder
//
// See rfc 2083 for technical details.
//
// https://www.ietf.org/rfc/rfc2083.txt
//
// Define WITH_Z_LOG and/or WITH_PNG_LOG to get lots of debug output.
//


#ifdef WITH_Z_LOG
#include <stdio.h>
#define Z_LOG(a...) fprintf(stderr, a)
#else
#define Z_LOG(a...)
#endif

#ifdef WITH_PNG_LOG
#include <stdio.h>
#define PNG_LOG(a...) fprintf(stderr, a)
#else
#define PNG_LOG(a...)
#endif


#define Z_MAX_CODE_BITS		15
#define Z_MAX_SYMS		288
#define Z_ADLER32_MODULO	65521
#define Z_WINDOW_SIZE		0x8000

#define PNG_CHUNK_IHDR		0x49484452
#define PNG_CHUNK_IDAT		0x49444154

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

typedef struct {
  unsigned type, len, pos, crc;
  uint8_t *buf;
} png_chunk_t;

typedef struct {
  png_chunk_t chunk;
  unsigned width, height, pixel_bytes;
  unsigned x, y, pixel_byte, filter;
  unsigned got_filter:1;
} png_data_t;

typedef struct {
  struct {
    // max_1: max value + 1
    unsigned min, max_1, ofs;
  } codes[Z_MAX_CODE_BITS + 1];
  uint16_t syms[Z_MAX_SYMS];
} z_huff_table_t;

typedef struct {
  struct {
    uint8_t *buf;
    unsigned len, pos, bits, val;
  } input;
  struct {
    uint8_t *buf;
    unsigned len, pos;
  } output;
  struct {
    uint8_t *buf;
    unsigned pos;
  } window;
  struct {
    unsigned low, high;
  } adler32;
  unsigned bad;
  z_huff_table_t huff_lit, huff_dist, huff_clen;
  png_data_t png;
} z_inflate_state_t;


void z_adler32_update(z_inflate_state_t *inflate_state, unsigned val);
void z_out_byte(z_inflate_state_t *inflate_state, unsigned val);
void z_put_byte(z_inflate_state_t *inflate_state, unsigned val);
void z_copy_bytes(z_inflate_state_t *inflate_state, unsigned dist, unsigned len);
unsigned z_get_byte(z_inflate_state_t *inflate_state);
unsigned z_get_bits(z_inflate_state_t *inflate_state, unsigned bits);
unsigned z_get_code(z_inflate_state_t *inflate_state, z_huff_table_t *huff);
void z_init_huff_table(z_inflate_state_t *inflate_state, z_huff_table_t *huff, unsigned values, uint8_t code_bits[]);
void z_setup_fixed_huff_table(z_inflate_state_t *inflate_state);
void z_setup_dynamic_huff_table(z_inflate_state_t *inflate_state);
void z_inflate_compressed_block(z_inflate_state_t *inflate_state);
void z_inflate_uncompressed_block(z_inflate_state_t *inflate_state);
void z_inflate(z_inflate_state_t *inflate_state);

unsigned png_paeth(int a, int b, int c);
uint32_t png_get_uint32(uint8_t *buf);
png_chunk_t *png_get_chunk(z_inflate_state_t *inflate_state);
void png_get_size(z_inflate_state_t *inflate_state);


void z_adler32_update(z_inflate_state_t *inflate_state, unsigned val)
{
  inflate_state->adler32.low += val;
  if(inflate_state->adler32.low >= Z_ADLER32_MODULO) inflate_state->adler32.low -= Z_ADLER32_MODULO;
  inflate_state->adler32.high += inflate_state->adler32.low;
  if(inflate_state->adler32.high >= Z_ADLER32_MODULO) inflate_state->adler32.high -= Z_ADLER32_MODULO;
}


void z_out_byte(z_inflate_state_t *inflate_state, unsigned val)
{
  png_data_t *png = &inflate_state->png;

  if(png->x == 0 && png->pixel_byte == 0 && !png->got_filter) {
    png->got_filter = 1;
    png->filter = val;

    PNG_LOG("+++ filter[%u] = %u\n", png->y, png->filter);

    return;
  }

  // take care of rgb ordering:
  //   - png order: r, g, b, (a)
  //   - our order: b, g, r, a
  static const uint8_t order[4] = { 2, 0, 2, 0 };

  unsigned pos = inflate_state->output.pos ^ order[inflate_state->output.pos & 3];

  // a = left, b = up; c = left + up
  // internal pixel size is always 4
  unsigned a = png->x ? inflate_state->output.buf[pos - 4] : 0;
  unsigned b = png->y ? inflate_state->output.buf[pos - 4 * png->width] : 0;
  unsigned c = png->x && png->y ? inflate_state->output.buf[pos - 4 * png->width - 4] : 0;

  switch(png->filter) {
    case 1:
      val += a;
      break;

    case 2:
      val += b;
      break;

    case 3:
      val += (a + b) >> 1;
      break;

    case 4:
      val += png_paeth((int) a, (int) b, (int) c);
      break;
  }

  if(inflate_state->output.pos < inflate_state->output.len) {
    // watch out: pos is not inflate_state->output.pos
    inflate_state->output.buf[pos] = val;
    inflate_state->output.pos++;
  }
  else {
    inflate_state->bad = __LINE__;
  }

  if(++png->pixel_byte == png->pixel_bytes) {
    png->pixel_byte = 0;
    png->x++;
    if(png->pixel_bytes == 3) {
      // skip alpha
      inflate_state->output.pos++;
    }
    if(png->x == png->width) {
      png->x = 0;
      png->y++;
    }
  }

  png->got_filter = 0;
}


void z_put_byte(z_inflate_state_t *inflate_state, unsigned val)
{
  inflate_state->window.buf[inflate_state->window.pos] = val;
  inflate_state->window.pos = (inflate_state->window.pos + 1) & (Z_WINDOW_SIZE - 1);

  z_adler32_update(inflate_state, val);

  z_out_byte(inflate_state, val);
}


void z_copy_bytes(z_inflate_state_t *inflate_state, unsigned dist, unsigned len)
{
  Z_LOG("+++ copy: dist = %u, len = %u\n", dist, len);

  while(len--) {
    z_put_byte(inflate_state, inflate_state->window.buf[(inflate_state->window.pos - dist) & (Z_WINDOW_SIZE - 1)]);
  }
}


unsigned z_get_byte(z_inflate_state_t *inflate_state)
{
  inflate_state->input.bits = 0;

  png_chunk_t *chunk = &inflate_state->png.chunk;

  if(chunk->pos >= chunk->len) {
    while((chunk = png_get_chunk(inflate_state)) || chunk->pos >= chunk->len) {
      if(chunk->type == PNG_CHUNK_IDAT) {
        break;
      }
    }
    if(!chunk) inflate_state->bad = __LINE__;
  }

  return inflate_state->bad ? 0 : chunk->buf[chunk->pos++];
}


unsigned z_get_bits(z_inflate_state_t *inflate_state, unsigned bits)
{
  unsigned val = 0;

  for(unsigned bit = bits; bit; bit--) {
    if(!inflate_state->input.bits) {
      inflate_state->input.val = z_get_byte(inflate_state);
      inflate_state->input.bits = 8;
    }
    val += (inflate_state->input.val & 1) << (bits - bit);
    inflate_state->input.val >>= 1;
    inflate_state->input.bits--;
  }

  return val;
}


unsigned z_get_code(z_inflate_state_t *inflate_state, z_huff_table_t *huff)
{
  unsigned code = 0;

  for(unsigned bits = 1; bits <= Z_MAX_CODE_BITS; bits++) {
     code = (code << 1) + z_get_bits(inflate_state, 1);
     // Z_LOG("+++ bits = %u, code = 0b%b\n", bits, code);
     if(code >= huff->codes[bits].min && code < huff->codes[bits].max_1) {
       return huff->syms[huff->codes[bits].ofs + code - huff->codes[bits].min];
     }
  }

  inflate_state->bad = __LINE__;

  return 0;
}


void z_init_huff_table(z_inflate_state_t *inflate_state, z_huff_table_t *huff, unsigned values, uint8_t code_bits[])
{
  unsigned count[Z_MAX_CODE_BITS + 1] = { };
  unsigned pos[Z_MAX_CODE_BITS + 1] = { };

  *huff = (z_huff_table_t) { };

  for(unsigned u = 0; u < values; u++) {
    unsigned bits = code_bits[u];
    if(bits > Z_MAX_CODE_BITS) {
      inflate_state->bad = __LINE__;
      return;
    }
    if(bits) count[bits]++;
  }

  unsigned code = 0;
  unsigned ofs = 0;
  for(unsigned bits = 1; bits <= Z_MAX_CODE_BITS; bits++) {
    code = (code + count[bits - 1]) << 1;
    if(count[bits]) {
      huff->codes[bits].ofs = pos[bits] = ofs;
      huff->codes[bits].min = code;
      huff->codes[bits].max_1 = code + count[bits];
    }
    ofs += count[bits];
  }

  for(unsigned u = 0; u < values; u++ ) {
    unsigned bits = code_bits[u];
    if(!bits) continue;
    unsigned idx = pos[bits]++;
    if(idx >= Z_MAX_SYMS) {
      inflate_state->bad = __LINE__;
      return;
    }
    huff->syms[idx] = u;
  }

#ifdef WITH_Z_LOG
  Z_LOG("+++ Huffman table +++\n");

  for(unsigned u = 0; u <= Z_MAX_CODE_BITS; u++) {
    if(count[u]) Z_LOG("+++ bits[%2u] = %3u\n", u, count[u]);
  }

  for(unsigned u = 0; u <= Z_MAX_CODE_BITS; u++) {
    if(huff->codes[u].max_1) {
      Z_LOG(
        "+++ tbl.bits[%2u]: ofs %3u, min %10b, max %10b\n",
        u, huff->codes[u].ofs, huff->codes[u].min, huff->codes[u].max_1 - 1
      );
    }
  }

  unsigned lines = values < 32 ? values : 32;

  for(unsigned line = 0; line < lines; line++) {
    Z_LOG("+++ ");
    for(unsigned col = 0; col < 9; col++) {
      unsigned u = col * lines + line;
      if(u < values) {
        Z_LOG("  [%3u] = %3u", u, huff->syms[u]);
      }
    }
    Z_LOG("\n");
  }
#endif
}


void z_setup_fixed_huff_table(z_inflate_state_t *inflate_state)
{
  unsigned u = 0;
  uint8_t code_len[288];

  while(u < 144) code_len[u++] = 8;
  while(u < 256) code_len[u++] = 9;
  while(u < 280) code_len[u++] = 7;
  while(u < 288) code_len[u++] = 8;
  z_init_huff_table(inflate_state, &inflate_state->huff_lit, 288, code_len);

  if(inflate_state->bad) return;

  for(u = 0; u < 32; ) code_len[u++] = 5;
  z_init_huff_table(inflate_state, &inflate_state->huff_dist, 32, code_len);
}


void z_setup_dynamic_huff_table(z_inflate_state_t *inflate_state)
{
  static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

  unsigned hlit = z_get_bits(inflate_state, 5) + 257;
  unsigned hdist = z_get_bits(inflate_state, 5) + 1;
  unsigned hclen = z_get_bits(inflate_state, 4) + 4;

  Z_LOG("+++ hlit %u\n", hlit);
  Z_LOG("+++ hdist %u\n", hdist);
  Z_LOG("+++ hclen %u\n", hclen);

  if(!inflate_state->bad) {
    if(hlit > 286 || hclen > 19) {
      inflate_state->bad = __LINE__;
      return;
    }
  }

  uint8_t clen_bits[19] = { };

  for(unsigned u = 0; u < hclen; u++) {
    clen_bits[order[u]] = z_get_bits(inflate_state, 3);
  }

  z_init_huff_table(inflate_state, &inflate_state->huff_clen, sizeof clen_bits / sizeof *clen_bits, clen_bits);

  unsigned all_len = hlit + hdist;

  // not initialized - the following loop sets all values
  uint8_t all_bits[all_len];

  Z_LOG("+++ all_len = %3u\n", all_len);

  for(unsigned u = 0; u < all_len;) {
    unsigned bits = z_get_code(inflate_state, &inflate_state->huff_clen);

    if(bits < 16) {
      Z_LOG("+++ [%3u] - bits = %2u\n", u, bits);

      all_bits[u++] = bits;
      continue;
    }

    unsigned repeat = 0;

    switch(bits) {
      case 16:
        if(u == 0) {
          inflate_state->bad = __LINE__;
          return;
        }
        bits = all_bits[u - 1];
        repeat = 3 + z_get_bits(inflate_state, 2);
        break;

      case 17:
        repeat = 3 + z_get_bits(inflate_state, 3);
        bits = 0;
        break;

      case 18:
        repeat = 11 + z_get_bits(inflate_state, 7);
        bits = 0;
        break;
    }

    Z_LOG("+++ [%3u] - repeat = %3u, bits = %2u\n", u, repeat, bits);

    while(repeat--) {
      all_bits[u++] = bits;
      if(u > all_len) {
        inflate_state->bad = __LINE__;
        return;
      }
    }
  }

  if(inflate_state->bad) return;

  z_init_huff_table(inflate_state, &inflate_state->huff_lit, hlit, all_bits);
  z_init_huff_table(inflate_state, &inflate_state->huff_dist, hdist, all_bits + hlit);
}


void z_inflate_compressed_block(z_inflate_state_t *inflate_state)
{
  // same length as len_extra_bits[]
  static const uint8_t len_base[29] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 255
  };

  // same length as len_base[]
  static const uint8_t len_extra_bits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
  };

  // same length as dist_extra_bits[]
  static const uint16_t dist_base[30] = {
    0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576
  };

  // same length as dist_base[]
  static const uint8_t dist_extra_bits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
  };

  while(!inflate_state->bad) {
    unsigned sym = z_get_code(inflate_state, &inflate_state->huff_lit);

    Z_LOG("+++ sym = 0x%x\n", sym);

    // literal
    if(sym < 256) {
      z_put_byte(inflate_state, sym);
      continue;
    }

    // block end
    if(sym == 256) return;

    // length code
    unsigned len_code = sym - 257;

    Z_LOG("+++ len code = %u\n", len_code);

    if(len_code >= sizeof len_base / sizeof *len_base) {
      inflate_state->bad = __LINE__;
      return;
    }

    unsigned len = 3 + len_base[len_code] + z_get_bits(inflate_state, len_extra_bits[len_code]);

    Z_LOG("+++ len = %u\n", len);

    // distance code
    unsigned dist_code = z_get_code(inflate_state, &inflate_state->huff_dist);

    Z_LOG("+++ dist code = 0x%x\n", dist_code);

    if(dist_code >= sizeof dist_base / sizeof *dist_base) {
      inflate_state->bad = __LINE__;
      return;
    }

    unsigned dist = 1 + dist_base[dist_code] + z_get_bits(inflate_state, dist_extra_bits[dist_code]);

    Z_LOG("+++ dist = %u\n", dist);

    z_copy_bytes(inflate_state, dist, len);
  }
}


void z_inflate_uncompressed_block(z_inflate_state_t *inflate_state)
{
  unsigned len = z_get_byte(inflate_state);
  len += z_get_byte(inflate_state) << 8;

  unsigned nlen = z_get_byte(inflate_state);
  nlen += z_get_byte(inflate_state) << 8;

  Z_LOG("+++ len = 0x%04x, nlen = 0x%04x\n", len, nlen);

  if((len ^ nlen) != 0xffff) {
    inflate_state->bad = __LINE__;
    return;
  }

  while(len--) {
    z_put_byte(inflate_state, z_get_byte(inflate_state));
  }

  Z_LOG("\n");
}


void z_inflate(z_inflate_state_t *inflate_state)
{
  unsigned cmf = z_get_byte(inflate_state);
  unsigned flg = z_get_byte(inflate_state);

  // 0x78 = deflate, with 32k window
  //                0 = no FDICT
  //                                0 = FCHECK checksum valid
  if(cmf != 0x78 || (flg & 0x20) || ((cmf << 8) + flg) % 31) {
    inflate_state->bad = __LINE__;
  }

  inflate_state->adler32.low = 1;

  while(!inflate_state->bad) {
    unsigned bfinal = z_get_bits(inflate_state, 1);
    unsigned btype = z_get_bits(inflate_state, 2);

    Z_LOG("+++ bfinal %u\n", bfinal);
    Z_LOG("+++ bytpe %u\n", btype);

    switch(btype) {
      case 0:
        z_inflate_uncompressed_block(inflate_state);
        break;

      case 1:
      case 2:
        btype == 1 ? z_setup_fixed_huff_table(inflate_state) : z_setup_dynamic_huff_table(inflate_state);
        z_inflate_compressed_block(inflate_state);
        break;

      default:
        inflate_state->bad = __LINE__;
    }

    if(bfinal) break;
  }

  if(!inflate_state->bad) {
    unsigned adler32_stored = 0;
    for(unsigned u = 0; u < 4; u++) {
      adler32_stored = (adler32_stored << 8) + z_get_byte(inflate_state);
    }

    Z_LOG("+++ adler32 (stored) = 0x%08x\n", adler32_stored);

    unsigned adler32_calc = (inflate_state->adler32.high << 16) + inflate_state->adler32.low;

    Z_LOG("+++ adler32 (calc) = 0x%08x\n", adler32_calc);

    if(adler32_calc != adler32_stored) {
       inflate_state->bad = __LINE__;
    }
  }

  if(!inflate_state->bad && inflate_state->png.pixel_bytes == 4) {
    // invert alpha values (so that 0 = fully opaque)
    for(unsigned u = 3; u < inflate_state->output.pos; u += 4) {
      inflate_state->output.buf[u] = 255 - inflate_state->output.buf[u];
    }
  }

  return;
}


// use int, not unsigned; necessary for the calculation
unsigned png_paeth(int a, int b, int c)
{
  int p = a + b - c;

  int pa = p - a >= 0 ? p - a : a - p;
  int pb = p - b >= 0 ? p - b : b - p;
  int pc = p - c >= 0 ? p - c : c - p;

  if(pa <= pb && pa <= pc) return (unsigned) a;

  if(pb <= pc) return (unsigned) b;

  return (unsigned) c;
}


uint32_t png_get_uint32(uint8_t *buf)
{
  return ((unsigned) buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
}


png_chunk_t *png_get_chunk(z_inflate_state_t *inflate_state)
{
  unsigned pos = inflate_state->input.pos;

  if(pos + 12 > inflate_state->input.len) {
    inflate_state->bad = __LINE__;
    return 0;
  }

  unsigned len = png_get_uint32(inflate_state->input.buf + pos);

  unsigned type = png_get_uint32(inflate_state->input.buf + pos + 4);

  PNG_LOG("+++ chunk: type 0x%04x '%c%c%c%c', len %u\n", type, (type >> 24) & 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, len);

  if(inflate_state->input.pos + 12 + len > inflate_state->input.len) {
    inflate_state->bad = __LINE__;
    return 0;
  }

  unsigned crc = png_get_uint32(inflate_state->input.buf + pos + 8 + len);

  inflate_state->png.chunk.pos = 0;
  inflate_state->png.chunk.len = len;
  inflate_state->png.chunk.type = type;
  inflate_state->png.chunk.crc = crc;
  inflate_state->png.chunk.buf = inflate_state->input.buf + pos + 8;

  inflate_state->input.pos += 12 + len;

  return &inflate_state->png.chunk;
}


void png_get_size(z_inflate_state_t *inflate_state)
{
  if(inflate_state->input.len < 8) {
    inflate_state->bad = __LINE__;
    return;
  }

  uint64_t signature = ((uint64_t) png_get_uint32(inflate_state->input.buf) << 32) + png_get_uint32(inflate_state->input.buf + 4);

  PNG_LOG("+++ sig = 0x%016llx\n", (unsigned long long) signature);

  if(signature != 0x89504e470d0a1a0a) {
    inflate_state->bad = __LINE__;
    return;
  }

  inflate_state->input.pos = 8;

  png_chunk_t *chunk = png_get_chunk(inflate_state);

  if(!chunk) return;

  if(chunk->type != PNG_CHUNK_IHDR || chunk->len != 13) {
    inflate_state->bad = __LINE__;
    return;
  }

  unsigned width = png_get_uint32(chunk->buf);
  unsigned height = png_get_uint32(chunk->buf + 4);

  unsigned bits = chunk->buf[8];
  unsigned color_type = chunk->buf[9];
  unsigned compression = chunk->buf[10];
  unsigned filter = chunk->buf[11];
  unsigned interlace = chunk->buf[12];

  chunk->pos = chunk->len;

  PNG_LOG(
    "+++ width %u, height %u, bits %u, color_type %u, compression %u, filter %u, interlace %u\n",
    width, height, bits, color_type, compression, filter, interlace
  );

  if(bits != 8 || !(color_type == 2 || color_type == 6) || compression != 0 || filter != 0 || interlace != 0) {
    inflate_state->bad = __LINE__;
    return;
  }

  inflate_state->png.width = width;
  inflate_state->png.height = height;
  inflate_state->png.pixel_bytes = color_type == 2 ? 3 : 4;
}


#include <stdio.h>
#include <stdlib.h>

#define TEST_BUF_SIZE	10*1000*1000

int main()
{
  z_inflate_state_t *inflate_state = &(z_inflate_state_t) { };

  inflate_state->input.len = TEST_BUF_SIZE;
  inflate_state->input.buf = calloc(1, TEST_BUF_SIZE);

  inflate_state->window.buf = calloc(1, Z_WINDOW_SIZE);

  for(int i; (i = getchar()) >= 0; ) {
    inflate_state->input.buf[inflate_state->input.pos++] = i;
    if(inflate_state->input.pos >= inflate_state->input.len) break;
  }

  inflate_state->input.len = inflate_state->input.pos;
  inflate_state->input.pos = 0;

  png_get_size(inflate_state);

  if(inflate_state->png.width && inflate_state->png.height) {
    inflate_state->output.len = inflate_state->png.width * inflate_state->png.height * 4;
    inflate_state->output.buf = calloc(1, inflate_state->output.len);
  }

  z_inflate(inflate_state);

  Z_LOG("+++ in %d, out %d\n", inflate_state->input.pos, inflate_state->output.pos);

  if(inflate_state->bad) {
    Z_LOG("+++ invalid data at line %u +++\n", inflate_state->bad);
  }

  png_data_t *png = &inflate_state->png;

  // output in ImageMagick 'debug' format
  printf("# ImageMagick pixel debugging: %u,%u,255,srgb", png->width, png->height);
  if(png->pixel_bytes == 4) printf("a");
  printf("\n");

  for(unsigned y = 0; y < png->height; y++) {
    for(unsigned x = 0; x < png->width; x++) {
      unsigned pos = (png->width * y + x) * 4;
      unsigned r = inflate_state->output.buf[pos + 2];
      unsigned g = inflate_state->output.buf[pos + 1];
      unsigned b = inflate_state->output.buf[pos + 0];
      unsigned a = inflate_state->output.buf[pos + 3];
      printf("%u,%u: %u,%u,%u ", x, y, 0x101 * r, 0x101 * g, 0x101 * b);
      if(png->pixel_bytes == 4) printf(",%u ", 0x101 * (255 - a));
      printf("\n");
    }
  }

  return inflate_state->bad ? 1 : 0;
}
