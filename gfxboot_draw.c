#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// drawing functions
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void gfx_screen_update(obj_id_t canvas_id, area_t area);
static void gfx_canvas_update(obj_id_t canvas_id, area_t area);

static obj_id_t gfx_font_render_glyph(gstate_t *gstate, area_t *geo, unsigned c);
static obj_id_t gfx_font_render_font1_glyph(gstate_t *gstate, font_t *font, area_t *geo, unsigned c);
static obj_id_t gfx_font_render_font2_glyph(gstate_t *gstate, font_t *font, area_t *geo, unsigned c);
static unsigned read_unsigned_bits(uint8_t *buf, unsigned *bit_ofs, unsigned bits);
static int read_signed_bits(uint8_t *buf, unsigned *bit_ofs, unsigned bits);

static color_t gfx_color_merge(color_t dst, color_t src);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_screen_update(obj_id_t canvas_id, area_t area)
{
  int i, j;
  uint8_t *r8, *rc8;
  color_t c;
  uint32_t rc;
  uint32_t rm, gm, bm;
  int rs, gs, bs;
  int rp, gp, bp;
  int rsize;

  canvas_t *virt_fb = gfx_obj_canvas_ptr(canvas_id);
  if(!virt_fb) return;

  color_t *pixel = virt_fb->ptr;

  rm = (1u << gfxboot_data->screen.real.red.size) - 1;
  gm = (1u << gfxboot_data->screen.real.green.size) - 1;
  bm = (1u << gfxboot_data->screen.real.blue.size) - 1;

  rs = 24 - gfxboot_data->screen.real.red.size;
  gs = 16 - gfxboot_data->screen.real.green.size;
  bs = 8 - gfxboot_data->screen.real.blue.size;

  rp = gfxboot_data->screen.real.red.pos;
  gp = gfxboot_data->screen.real.green.pos;
  bp = gfxboot_data->screen.real.blue.pos;

  rsize = gfxboot_data->screen.real.bytes_per_pixel;

#if 0
  gfxboot_log(
    "rm = 0x%02x, gm = 0x%02x, bm = 0x%02x, rs = %d, gs = %d, bs = %d, rp = %d, gp = %d, bp = %d\n",
    rm, gm, bm, rs, gs, bs, rp, gp, bp
  );
#endif

  r8 = (uint8_t *) gfxboot_data->screen.real.ptr +
    area.y * gfxboot_data->screen.real.bytes_per_line +
    area.x * gfxboot_data->screen.real.bytes_per_pixel;

  pixel += area.y * virt_fb->geo.width + area.x;

  for(j = 0; j < area.height; j++, r8 += gfxboot_data->screen.real.bytes_per_line, pixel += virt_fb->geo.width) {
    for(rc8 = r8, i = 0; i < area.width; i++) {
      c = pixel[i];
      rc = (((c >> rs) & rm) << rp) + (((c >> gs) & gm) << gp) + (((c >> bs) & bm) << bp);
      switch(rsize) {
        case 4:
          *rc8++ = rc;
          rc >>= 8;
        case 3:
          *rc8++ = rc;
          rc >>= 8;
        case 2:
          *rc8++ = rc;
          rc >>= 8;
        case 1:
          *rc8++ = rc;
          rc >>= 8;
      }
    }
  }

  gfxboot_screen_update(area);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_canvas_update(obj_id_t canvas_id, area_t area)
{
  obj_id_t list_id = gfxboot_data->compose.list_id;
  array_t *list = gfx_obj_array_ptr(list_id);

  if(!list) return;

  int list_size = (int) list->size;

  for(int i = 0; i <= list_size; i++) {
    gstate_t *gstate = 0;
    if(i == list_size) {
      if(gfxboot_data->vm.debug.console.show) gstate = gfx_obj_gstate_ptr(gfxboot_data->console.gstate_id);
    }
    else {
      gstate = gfx_obj_gstate_ptr(gfx_obj_array_get(list_id, i));
    }
    if(!gstate) continue;
    if(gstate->canvas_id == canvas_id) {
      area.x += gstate->geo.x;
      area.y += gstate->geo.y;
      gfx_screen_compose(area);
      return;
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_screen_compose(area_t area)
{
  obj_id_t list_id = gfxboot_data->compose.list_id;
  array_t *list = gfx_obj_array_ptr(list_id);

  if(!list) return;

  gstate_t *target_gstate = gfx_obj_gstate_ptr(gfxboot_data->screen.gstate_id);

  if(!target_gstate) return;

  int list_size = (int) list->size;

  gfx_clip(&area, &target_gstate->geo);

  draw_mode_t blt_mode = dm_direct + dm_no_update;

  for(int i = 0; i <= list_size; i++) {
    gstate_t *gstate = 0;
    if(i == list_size) {
      if(gfxboot_data->vm.debug.console.show) gstate = gfx_obj_gstate_ptr(gfxboot_data->console.gstate_id);
    }
    else {
      gstate = gfx_obj_gstate_ptr(gfx_obj_array_get(list_id, i));
    }
    if(!gstate) continue;
    area_t dst_area = gstate->geo;
    area_t src_area = area;
    area_t diff = gfx_clip(&dst_area, &src_area);
    if(dst_area.width) {
      // gfxboot_serial(0, "XXX i = %d: +%dx%d_%dx%d %dx%d_%dx%d\n", i, diff.x, diff.y, diff.width, diff.height, dst_area.x, dst_area.y, dst_area.width, dst_area.height);
      src_area.x = diff.x;
      src_area.y = diff.y;
      src_area.width = dst_area.width;
      src_area.height = dst_area.height;
      // gfxboot_serial(0, "compose %d: src = %dx%d_%dx%d, dst = %dx%d_%dx%d\n", i, src_area.x, src_area.y, src_area.width, src_area.height, dst_area.x, dst_area.y, dst_area.width, dst_area.height);
      gfx_blt(blt_mode, target_gstate->canvas_id, dst_area, gstate->canvas_id, src_area);
      blt_mode = dm_merge + dm_no_update;
    }
  }

  gfx_screen_update(target_gstate->canvas_id, area);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_font_render_glyph(gstate_t *gstate, area_t *geo, unsigned c)
{
  font_t *font;
  obj_id_t font_id, glyph_id = 0;

  while(1) {
    for(font_id = GSTATE_TO_CANVAS(gstate)->font_id; (font = gfx_obj_font_ptr(font_id)); font_id = font->parent_id) {
      switch(font->type) {
        case 1:
          glyph_id = gfx_font_render_font1_glyph(gstate, font, geo, c);
          break;

        case 2:
          glyph_id = gfx_font_render_font2_glyph(gstate, font, geo, c);
          break;
      }
      if(glyph_id) return glyph_id;
    }

    if(c == 0xfffd) return 0;

    c = 0xfffd;
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_font_render_font1_glyph(gstate_t *gstate, font_t *font, area_t *geo, unsigned c)
{
  data_t *data = gfx_obj_mem_ptr(font->data_id);
  if(!data) return 0;

  char *uni = data->ptr + font->unimap.offset;
  char *uni_end = uni + font->unimap.size;

  int i, j;
  uint8_t *bitmap = 0;
  unsigned uni_len = font->unimap.size;
  unsigned idx;

  for(idx = 0; uni < uni_end; ) {
    i = gfx_utf8_dec(&uni, &uni_len);
    if(i == -0xff) {
      idx++;
      continue;
    }
    if(i == (int) c) {
      if(idx < font->glyphs) {
        bitmap = data->ptr + font->bitmap.offset + idx * font->glyph_size;
      }
      break;
    }
  }

  if(!bitmap) return 0;

  *geo = (area_t) { .x = 0, .y = 0, .width = font->width, .height = 0 };

  canvas_t *glyph = gfx_obj_canvas_ptr(font->glyph_id);

  if(
    !glyph ||
    !gfx_canvas_adjust_size(glyph, font->width, font->height)
  ) {
    return 0;
  }

  // gfxboot_log("char 0x%04x, idx %u, bitmap %p\n", c, idx, bitmap);

  // got bitmap, now go for it

  color_t fg = GSTATE_TO_CANVAS(gstate)->color;
  color_t bg = GSTATE_TO_CANVAS(gstate)->bg_color;

  color_t *col = glyph->ptr;
  uint8_t cb;

  for(j = 0; j < font->height; j++) {
    for(i = 0, cb = *bitmap++; i < font->width; i++) {
      *col++ = (cb & 0x80) ? fg : bg;
      cb <<= 1;
    }
  }

  return font->glyph_id;
}


#define GRAY_BITS	4
#define GRAY_BIT_COUNT	3
#define MAX_GRAY	((1 << GRAY_BITS) - 3)
#define REP_BG		(MAX_GRAY + 1)
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_font_render_font2_glyph(gstate_t *gstate, font_t *font, area_t *geo, unsigned c)
{
  data_t *data = gfx_obj_mem_ptr(font->data_id);
  if(!data) return 0;

  uint8_t *ofs_table = data->ptr + font->unimap.offset;
  unsigned u, ofs = 0;

  for(u = 0; u < font->glyphs; u++) {
    unsigned g = gfx_read_le32(ofs_table + 5 * u) & ((1 << 21) - 1);
    if(g == c) {
      ofs = gfx_read_le32(ofs_table + 5 * u + 1);
      ofs >>= 21 - 8;
      break;
    }
  }

  // gfxboot_serial(0, "font2: char %u, ofs %u\n", c, ofs);

  if(
    u == font->glyphs || ofs < font->bitmap.offset || ofs >= data->size
  ) {
    return 0;
  }

  uint8_t *glyph_data = data->ptr + ofs;
  unsigned bit_ofs = 0;

  unsigned type = read_unsigned_bits(glyph_data, &bit_ofs, 2);

  if(type != 1) return 0;

  unsigned bits = read_unsigned_bits(glyph_data, &bit_ofs, 3) + 1;

  int bitmap_width = (int) read_unsigned_bits(glyph_data, &bit_ofs, bits);
  int bitmap_height = (int) read_unsigned_bits(glyph_data, &bit_ofs, bits);
  int x_ofs = read_signed_bits(glyph_data, &bit_ofs, bits);
  int y_ofs = read_signed_bits(glyph_data, &bit_ofs, bits);
  int x_advance = read_signed_bits(glyph_data, &bit_ofs, bits);

#if 0
  gfxboot_serial(0, "font2: bits %u, bitmap %dx%d, ofs %dx%d, advance %d\n",
    bits, bitmap_width, bitmap_height, x_ofs, y_ofs, x_advance
  );
#endif

  canvas_t *glyph = gfx_obj_canvas_ptr(font->glyph_id);

  if(
    !glyph ||
    !gfx_canvas_adjust_size(glyph, bitmap_width, bitmap_height)
  ) {
    return 0;
  }

  // recalculate offset relative to top of glyph
  int d_y = font->height - font->baseline - y_ofs - bitmap_height;

  *geo = (area_t) { .x = x_ofs, .y = d_y, .width = x_advance, .height = 0 };

  unsigned len = (unsigned) (bitmap_height * bitmap_width);

  static color_t last_fg = COLOR(0, 0, 0, 0);
  static color_t color_map[MAX_GRAY + 1] = { };
  static int first_time_ever = 1;

  union {
    color_t c;
    struct {
      uint8_t b, g, r, a;
    } __attribute__ ((packed));
  } __attribute__ ((packed)) fg, tmp;

  fg.c = GSTATE_TO_CANVAS(gstate)->color;

  // if drawing color changed, recalculate color mapping table
  if(fg.c != last_fg || first_time_ever) {
    first_time_ever = 0;
    last_fg = fg.c;

    for(int i = 0; i <= MAX_GRAY; i++) {
      tmp.c = fg.c;
      tmp.a = 255 - i * (255 - fg.a) / MAX_GRAY;
      color_map[i] = tmp.c;
    }
  }

  for(u = 0; u < len;) {
    unsigned lc = read_unsigned_bits(glyph_data, &bit_ofs, GRAY_BITS);
    // gfxboot_serial(0, "(%u)", lc);
    if(lc <= MAX_GRAY) {
      glyph->ptr[u++] = color_map[lc];
      continue;
    }
    lc = lc == REP_BG ? 0 : MAX_GRAY;
    unsigned lc_cnt = read_unsigned_bits(glyph_data, &bit_ofs, GRAY_BIT_COUNT) + 3;
    // gfxboot_serial(0, "(%u)", lc_cnt);
    while(u < len && lc_cnt--) glyph->ptr[u++] = color_map[lc];
  }

  return font->glyph_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_console_putc(unsigned c, int update_pos)
{
  gstate_t *gstate = gfx_obj_gstate_ptr(gfxboot_data->console.gstate_id);

  if(!gstate) return;

  switch(c) {
    case 0x08:
      if(update_pos) {
        gfx_putc(gstate, ' ', 0);
        if(gstate->cursor.x >= gstate->cursor.width) {
          gstate->cursor.x -= gstate->cursor.width;
        }
        else {
          gstate->cursor.x = 0;
        }
      }
      break;

    case 0x0a:
      if(update_pos) {
        gstate->cursor.x = 0;
        gstate->cursor.y += gstate->cursor.height;
      }

      area_t src_area = gstate->region;
      area_t dst_area = gstate->region;

      dst_area.height = src_area.height -= gstate->cursor.height;
      src_area.y += gstate->cursor.height;

      if(gstate->cursor.y + gstate->cursor.height > gstate->region.height) {
        gstate->cursor.y -= gstate->cursor.height;

        // scroll up
        gfx_blt(GSTATE_TO_CANVAS(gstate)->draw_mode, gstate->canvas_id, dst_area, gstate->canvas_id, src_area);

        gfx_rect(
          gstate,
          0,
          gstate->region.height - gstate->cursor.height,
          gstate->region.width,
          gstate->cursor.height,
          GSTATE_TO_CANVAS(gstate)->bg_color
        );
      }
      break;

    case 0x0d:
      if(update_pos) {
        gstate->cursor.x = 0;
      }
      break;

    default:
      gfx_putc(gstate, c, update_pos);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_console_puts(char *s)
{
  int c;

  while((c = gfx_utf8_dec(&s, 0))) {
    gfx_console_putc((unsigned) c, 1);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_putc(gstate_t *gstate, unsigned c, int update_pos)
{
  obj_id_t glyph_id;
  area_t geo;

  if((glyph_id = gfx_font_render_glyph(gstate, &geo, c))) {
    canvas_t *glyph = gfx_obj_canvas_ptr(glyph_id);
    if(!glyph) return;

    area_t area = {
      .x = gstate->region.x + gstate->cursor.x + geo.x,
      .y = gstate->region.y + gstate->cursor.y + geo.y,
      .width = glyph->geo.width,
      .height = glyph->geo.height
    };

    area_t glyph_area = {
      .x = 0,
      .y = 0,
      .width = glyph->geo.width,
      .height = glyph->geo.height
    };

#if 1
    gfxboot_serial(1, "putc 0x%2x - area %dx%d_%dx%d",
      c, area.x, area.y, area.width, area.height
    );
#endif

    area_t diff = gfx_clip(&area, &gstate->region);

#if 1
    gfxboot_serial(1, " - clipped area %dx%d_%dx%d",
      area.x, area.y, area.width, area.height
    );
    gfxboot_serial(1, " - diff %dx%d_%dx%d\n",
      diff.x, diff.y, diff.width, diff.height
    );
#endif

    ADD_AREA(glyph_area, diff);

    gfx_blt(GSTATE_TO_CANVAS(gstate)->draw_mode, gstate->canvas_id, area, glyph_id, glyph_area);

    if(update_pos) gstate->cursor.x += geo.width;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_puts(gstate_t *gstate, char *s, unsigned len)
{
  int c;
  int start = gstate->cursor.x;

  while(len) {
    c = gfx_utf8_dec(&s, &len);
    if(c < 0) c = 0xfffd;

    switch(c) {
      case 0x0a:
        gstate->cursor.y += gstate->cursor.height;

      case 0x0d:
        gstate->cursor.x = start;
        break;

      default:
        gfx_putc(gstate, (unsigned) c, 1);
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// clip area1 to difference of area1 and area2
//
// return difference (diff = area1_clipped - area1_unclipped)
//
// Notes:
//   - diff.x, diff.y >= 0
//   - diff.width, diff.height <= 0
//   - if the clipped area is 0, both width and height will be set to 0
//
area_t gfx_clip(area_t *area1, area_t *area2)
{
  area_t diff = {};
  int d;

  d = area1->x - area2->x;

  if(d < 0) {
    diff.x = -d;
    diff.width = d;
  }

  d = area1->y - area2->y;

  if(d < 0) {
    diff.y = -d;
    diff.height = d;
  }

  d = area2->x + area2->width - (area1->x + area1->width);

  if(d < 0) diff.width += d;

  d = area2->y + area2->height - (area1->y + area1->height);

  if(d < 0) diff.height += d;

  area1->x += diff.x;
  area1->y += diff.y;
  area1->width += diff.width;
  area1->height += diff.height;

  if(area1->width <= 0 || area1->height <= 0) {
    diff.width -= area1->width;
    area1->width = 0;
    diff.height -= area1->height;
    area1->height = 0;
  }

  return diff;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_blt(draw_mode_t mode, obj_id_t dst_id, area_t dst_area, obj_id_t src_id, area_t src_area)
{
  canvas_t *dst_c = gfx_obj_canvas_ptr(dst_id);
  canvas_t *src_c = gfx_obj_canvas_ptr(src_id);

  gfxboot_serial(4, "dst #%d (%dx%d_%dx%d), src #%d (%dx%d_%dx%d)\n",
    OBJ_ID2IDX(dst_id),
    dst_area.x, dst_area.y, dst_area.width, dst_area.height,
    OBJ_ID2IDX(src_id),
    src_area.x, src_area.y, src_area.width, src_area.height
  );

  if(!dst_c || !src_c) return;

  draw_mode_t mode_no_update = mode & dm_no_update;
  mode &= dm_no_update - 1;

  dst_area.width = MIN(src_area.width, dst_area.width);
  dst_area.height = MIN(src_area.height, dst_area.height);

#if 0
  // additional clipping needed?
  area_t tmp_area, diff;

  tmp_area = (area_t) { .width = dst_c->width, .height = dst_c->height };

  diff = gfx_clip(&dst_area, &tmp_area);

  ADD_AREA(src_area, diff);

  tmp_area = (area_t) { .width = src_c->width, .height = src_c->height };

  diff = gfx_clip(&src_area, &tmp_area);

  ADD_AREA(dst_area, diff);
#endif

  if(dst_area.width <= 0 || dst_area.height <= 0) return;

  color_t *dst_pixel = dst_c->ptr;
  color_t *src_pixel = src_c->ptr;

  dst_pixel += dst_area.y * dst_c->geo.width + dst_area.x;
  src_pixel += src_area.y * src_c->geo.width + src_area.x;

  if(mode == dm_direct) {
    int i;
    for(i = 0; i < dst_area.height; i++, dst_pixel += dst_c->geo.width, src_pixel += src_c->geo.width) {
      gfx_memcpy(dst_pixel, src_pixel, (unsigned) dst_area.width * COLOR_BYTES);
    }
  }
  else if(mode == dm_merge) {
    int i, j;
    for(j = 0; j < dst_area.height; j++, dst_pixel += dst_c->geo.width, src_pixel += src_c->geo.width) {
      for(i = 0; i < dst_area.width; i++) {
        dst_pixel[i] = gfx_color_merge(dst_pixel[i], src_pixel[i]);
      }
    }
  }

  if(!mode_no_update) gfx_canvas_update(dst_id, dst_area);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
color_t gfx_color_merge(color_t dst, color_t src)
{
  union {
    color_t c;
    struct {
      uint8_t b, g, r, a;
    } __attribute__ ((packed));
  } __attribute__ ((packed)) d, s;

  d.c = dst;
  s.c = src;

  if(s.a == 0xff) return d.c;
  if(s.a == 0) {
    s.a = d.a;
    return s.c;
  }

  s.b += (((int) d.b - s.b + 1) * s.a) >> 8;
  s.g += (((int) d.g - s.g + 1) * s.a) >> 8;
  s.r += (((int) d.r - s.r + 1) * s.a) >> 8;

#if 0
  // slightly more correct would be
  s.b += (((int) d.b - s.b + (d.b > s.b ? 1 : 0)) * s.a) >> 8;
  s.g += (((int) d.g - s.g + (d.g > s.g ? 1 : 0)) * s.a) >> 8;
  s.r += (((int) d.r - s.r + (d.r > s.r ? 1 : 0)) * s.a) >> 8;
#endif

  s.a = d.a;

  return s.c;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_getpixel(gstate_t *gstate, int x, int y, color_t *color)
{
  int ok = 0;

  canvas_t *canvas = gfx_obj_canvas_ptr(gstate->canvas_id);

  if(!canvas) return 0;

  if(x >= 0 && y >= 0 && x < gstate->region.width && y < gstate->region.height) {
    x += gstate->region.x;
    y += gstate->region.y;
    if(x >= 0 && y >= 0 && x < canvas->geo.width && y < canvas->geo.height) {
      *color = canvas->ptr[x + y * canvas->geo.width];
      ok = 1;
    }
  }

  return ok;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_putpixel(gstate_t *gstate, int x, int y, color_t color)
{
  draw_mode_t mode_no_update = GSTATE_TO_CANVAS(gstate)->draw_mode & dm_no_update;
  draw_mode_t mode = GSTATE_TO_CANVAS(gstate)->draw_mode & (dm_no_update - 1);

  // gfxboot_serial(0, "X putpixel %dx%d\n", x, y);

  canvas_t *canvas = gfx_obj_canvas_ptr(gstate->canvas_id);

  if(!canvas) return;

  if(x >= 0 && y >= 0 && x < gstate->region.width && y < gstate->region.height) {
    x += gstate->region.x;
    y += gstate->region.y;
    if(x >= 0 && y >= 0 && x < canvas->geo.width && y < canvas->geo.height) {
      int ofs = x + y * canvas->geo.width;
      if(mode == dm_direct) {
        canvas->ptr[ofs] = color;
      }
      else if(mode == dm_merge) {
        canvas->ptr[ofs] = gfx_color_merge(canvas->ptr[ofs], color);
      }

      if(!mode_no_update) gfx_canvas_update(gstate->canvas_id, (area_t) { .x = x, .y = y, .width = 1, .height = 1 });
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_line(gstate_t *gstate, int x0, int y0, int x1, int y1, color_t color)
{
  if(x1 < x0) {
    int tmp;
    tmp = x0; x0 = x1; x1 = tmp;
    tmp = y0; y0 = y1; y1 = tmp;
  }

  int dx = x1 - x0;
  int dy = y1 - y0;
  int acc;

  // shortcut for horizontal and vertical lines
  if(dy == 0 || dx == 0) {
    if(dy < 0) {
      int tmp = y0; y0 = y1; y1 = tmp;
      dy = -dy;
    }
    if(dx) {
      gfx_rect(gstate, x0, y0, dx + 1, 1, color);
    }
    else if(dy) {
      gfx_rect(gstate, x0, y0, 1, dy + 1, color);
    }

    return;
  }

  if(dy >= 0) {
    if(dy <= dx) {
      for(acc = -(dx / 2); x0 <= x1; x0++) {
        gfx_putpixel(gstate, x0, y0, color);
        acc += dy;
        if(acc >= 0) {
          acc -= dx;
          y0++;
        }
      }
    }
    else {
      for(acc = -(dy / 2); y0 <= y1; y0++) {
        gfx_putpixel(gstate, x0, y0, color);
        acc += dx;
        if(acc >= 0) {
          acc -= dy;
          x0++;
        }
      }
    }
  }
  else {
    dy = -dy;
    if(dy <= dx) {
      for(acc = -(dx / 2); x0 <= x1; x0++) {
        gfx_putpixel(gstate, x0, y0, color);
        acc += dy;
        if(acc >= 0) {
          acc -= dx;
          y0--;
        }
      }
    }
    else {
      for(acc = -(dy / 2); y0 >= y1; y0--) {
        gfx_putpixel(gstate, x0, y0, color);
        acc += dx;
        if(acc >= 0) {
          acc -= dy;
          x0++;
        }
      }
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_rect(gstate_t *gstate, int x, int y, int width, int height, color_t c)
{
  draw_mode_t mode_no_update = GSTATE_TO_CANVAS(gstate)->draw_mode & dm_no_update;
  draw_mode_t mode = GSTATE_TO_CANVAS(gstate)->draw_mode & (dm_no_update - 1);

  canvas_t *canvas = gfx_obj_canvas_ptr(gstate->canvas_id);

  if(!canvas) return;

  area_t area = {
    .x = gstate->region.x + x,
    .y = gstate->region.y + y,
    .width = width,
    .height = height
  };

#if 0
  gfxboot_serial(0, "rect before: %dx%d_%dx%d / %dx%d_%dx%d\n",
    area.x, area.y, area.width, area.height,
    gstate->region.x, gstate->region.y, gstate->region.width, gstate->region.height
  );
#endif

  gfx_clip(&area, &gstate->region);

#if 0
  gfxboot_serial(0, "rect after: %dx%d_%dx%d\n",
    area.x, area.y, area.width, area.height
  );
#endif

  color_t *pixel = canvas->ptr;

  pixel += area.y * canvas->geo.width + area.x;

  int i, j;

  if(mode == dm_direct) {
    for(j = 0; j < area.height; j++, pixel += canvas->geo.width) {
      for(i = 0; i < area.width; i++) {
        pixel[i] = c;
      }
    }
  }
  else if(mode == dm_merge) {
    for(j = 0; j < area.height; j++, pixel += canvas->geo.width) {
      for(i = 0; i < area.width; i++) {
        pixel[i] = gfx_color_merge(pixel[i], c);
      }
    }
  }

  if(!mode_no_update) gfx_canvas_update(gstate->canvas_id, area);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned read_unsigned_bits(uint8_t *buf, unsigned *bit_ofs, unsigned bits)
{
  unsigned rem, ptr;
  unsigned data = 0, dptr = 0;

  while(bits > 0) {
    ptr = *bit_ofs >> 3;
    rem = 8 - (*bit_ofs & 7);
    if(rem > bits) rem = bits;
    data += (((unsigned) buf[ptr] >> (*bit_ofs & 7)) & ((1u << rem) - 1)) << dptr;
    dptr += rem;
    *bit_ofs += rem;
    bits -= rem;
  }

  return data;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int read_signed_bits(uint8_t *buf, unsigned *bit_ofs, unsigned bits)
{
  int i;

  i = (int) read_unsigned_bits(buf, bit_ofs, bits);

  if(bits == 0) return i;

  if((i & (1 << (bits - 1)))) {
    i += -1 << bits;
  }

  return i;
}
