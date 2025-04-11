// Implementation of zlib decompression.
//
// See rfc 1950 and rfc 1951 for technical details.
//
// https://www.ietf.org/rfc/rfc1950.txt
// https://www.ietf.org/rfc/rfc1951.txt
//
// Define WITH_Z_LOG to get lots of debug output.
//

#define WITH_PNG_LOG	1

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

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

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
  unsigned bad;
  z_huff_table_t huff_lit, huff_dist, huff_clen;
} z_inflate_state_t;


unsigned z_adler32(z_inflate_state_t *inflate_state);
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


unsigned z_adler32(z_inflate_state_t *inflate_state)
{
  unsigned adler32_1 = 1;
  unsigned adler32_2 = 0;

  for(unsigned u = 0; u < inflate_state->output.pos; u++) {
    adler32_1 += inflate_state->output.buf[u];
    if(adler32_1 >= Z_ADLER32_MODULO) adler32_1 -= Z_ADLER32_MODULO;
    adler32_2 += adler32_1;
    if(adler32_2 >= Z_ADLER32_MODULO) adler32_2 -= Z_ADLER32_MODULO;
  }

  return (adler32_2 << 16) + adler32_1;
}


void z_put_byte(z_inflate_state_t *inflate_state, unsigned val)
{
  if(inflate_state->output.pos < inflate_state->output.len) {
    inflate_state->output.buf[inflate_state->output.pos++] = val;
  }
  else {
    inflate_state->bad = __LINE__;
  }

  Z_LOG("<%02x>", val);
}


void z_copy_bytes(z_inflate_state_t *inflate_state, unsigned dist, unsigned len)
{
  Z_LOG("+++ copy: dist = %u, len = %u\n", dist, len);

  if(inflate_state->output.pos < dist || inflate_state->output.pos + len > inflate_state->output.len) {
    inflate_state->bad = __LINE__;
    return;
  }

  for(unsigned u = inflate_state->output.pos - dist; len; len--) {
    Z_LOG("<%02x>", inflate_state->output.buf[u]);
    inflate_state->output.buf[inflate_state->output.pos++] = inflate_state->output.buf[u++];
  }
}


unsigned z_get_byte(z_inflate_state_t *inflate_state)
{
  inflate_state->input.bits = 0;

  if(inflate_state->input.pos < inflate_state->input.len) {
    return inflate_state->input.buf[inflate_state->input.pos++];
  }

  inflate_state->bad = __LINE__;

  return 0;
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

  uint8_t all_bits[all_len] = { };

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

    unsigned adler32_calc = z_adler32(inflate_state);

    Z_LOG("+++ adler32 (calc) = 0x%08x\n", adler32_calc);

    if(adler32_calc != adler32_stored) {
       inflate_state->bad = __LINE__;
    }
  }

  return;
}


#define PNG_CHUNK_IHDR		0x49484452
#define PNG_CHUNK_IEND		0x49454e44
#define PNG_CHUNK_IDAT		0x49444154
#define PNG_CHUNK_PLTE		0x504c5445

typedef struct {
  unsigned type, len, crc;
  uint8_t *buf;
} png_chunk_t;


typedef struct {
  struct {
    uint8_t *buf;
    unsigned len, pos;
    png_chunk_t chunk;
  } input;
  struct {
    uint8_t *buf;
    unsigned len, pos;
  } output;
  unsigned width, height, pixel_bytes;
  unsigned bad;
  z_inflate_state_t *z;
} png_image_state_t;


uint32_t png_get_uint32(uint8_t *buf)
{
  return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
}

png_chunk_t *png_get_chunk(png_image_state_t *image_state)
{
  unsigned pos = image_state->input.pos;

  if(pos + 12 > image_state->input.len) {
    image_state->bad = __LINE__;
    return NULL;
  }

  unsigned len = png_get_uint32(image_state->input.buf + pos);

  unsigned type = png_get_uint32(image_state->input.buf + pos + 4);

  PNG_LOG("+++ chunk: type 0x%04x '%c%c%c%c', len %u\n", type, (type >> 24) & 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, len);

  if(image_state->input.pos + 12 + len > image_state->input.len) {
    image_state->bad = __LINE__;
    return NULL;
  }

  unsigned crc = png_get_uint32(image_state->input.buf + pos + 8 + len);

  image_state->input.chunk.len = len;
  image_state->input.chunk.type = type;
  image_state->input.chunk.crc = crc;
  image_state->input.chunk.buf = image_state->input.buf + pos + 8;

  image_state->input.pos += 12 + len;

  return &image_state->input.chunk;
}


void png_decode(png_image_state_t *image_state)
{
  png_chunk_t *chunk;

  while((chunk = png_get_chunk(image_state))) {
    if(chunk->type == PNG_CHUNK_IEND) break;

    if(chunk->type == PNG_CHUNK_IDAT) {
      fwrite(chunk->buf, chunk->len, 1, stdout);
    }


  }
}


void png_get_size(png_image_state_t *image_state)
{
  if(image_state->input.len < 8) {
    image_state->bad = __LINE__;
    return;
  }

  uint64_t signature = ((uint64_t) png_get_uint32(image_state->input.buf) << 32) + png_get_uint32(image_state->input.buf + 4);

  PNG_LOG("+++ sig = 0x%016llx\n", (unsigned long long) signature);

  if(signature != 0x89504e470d0a1a0a) {
    image_state->bad = __LINE__;
    return;
  }

  image_state->input.pos = 8;

  png_chunk_t *chunk = png_get_chunk(image_state);

  if(!chunk) return;

  if(chunk->type != PNG_CHUNK_IHDR || chunk->len != 13) {
    image_state->bad = __LINE__;
    return;
  }

  unsigned width = png_get_uint32(chunk->buf);
  unsigned height = png_get_uint32(chunk->buf + 4);

  unsigned bits = chunk->buf[8];
  unsigned color_type = chunk->buf[9];
  unsigned compression = chunk->buf[10];
  unsigned filter = chunk->buf[11];
  unsigned interlace = chunk->buf[12];

  PNG_LOG(
    "+++ width %u, hwight %u, bits %u, color_type %u, compression %u, filter %u, interlace %u\n",
    width, height, bits, color_type, compression, filter, interlace
  );

  if(bits != 8 || !(color_type == 2 || color_type == 6) || compression != 0 || filter != 0 || interlace != 0) {
    image_state->bad = __LINE__;
    return;
  }

  image_state->width = width;
  image_state->height = height;
  image_state->pixel_bytes = color_type == 2 ? 3 : 4;
}


#include <stdio.h>
#include <stdlib.h>

#define TEST_BUF_SIZE	10*1000*1000

int main()
{
  png_image_state_t *image_state = &(png_image_state_t) { };

  image_state->input.len = TEST_BUF_SIZE;
  image_state->input.buf = calloc(1, TEST_BUF_SIZE);

  image_state->output.len = TEST_BUF_SIZE;
  image_state->output.buf = calloc(1, TEST_BUF_SIZE);

  for(int i; (i = getchar()) >= 0; ) {
    image_state->input.buf[image_state->input.pos++] = i;
    if(image_state->input.pos >= image_state->input.len) break;
  }

  image_state->input.len = image_state->input.pos;
  image_state->input.pos = 0;

  png_get_size(image_state);

  png_decode(image_state);

  PNG_LOG("+++ in %d, out %d\n", image_state->input.pos, image_state->output.pos);

  if(image_state->bad) {
    PNG_LOG("+++ invalid data at line %u +++\n", image_state->bad);
  }

  for(unsigned u = 0; u < image_state->output.pos; u++) {
    putchar(image_state->output.buf[u]);
  }

  return image_state->bad ? 1 : 0;
}
