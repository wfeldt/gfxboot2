#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// font


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_font_new()
{
  return gfx_obj_alloc(OTYPE_FONT, OBJ_FONT_SIZE());
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
font_t *gfx_obj_font_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_FONT) return 0;

  return (font_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_font_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  font_t *f = ptr->data.ptr;
  unsigned len = ptr->data.size;
  if(len != OBJ_FONT_SIZE()) {
    gfxboot_log("      <invalid data>\n");

    return 1;
  }

  if(!style.ref) {
    if(!style.inspect) return 0;

    if(f->type == 1) {
      gfxboot_log("glyphs %d, size %dx%d, line height %d", f->glyphs, f->width, f->height, f->line_height);
    }
    else if(f->type == 2) {
      gfxboot_log("glyphs %d, height %d, line height %d, base %d", f->glyphs, f->height, f->line_height, f->baseline);
    }
    if(f->parent_id) gfxboot_log(", parent %s", gfx_obj_id2str(f->parent_id));

    return 1;
  }

  if(style.dump) {
    canvas_t *glyph = gfx_obj_canvas_ptr(f->glyph_id);
    gfxboot_log("    type %u, glyphs %d\n", f->type, f->glyphs);
    gfxboot_log("    font size %dx%d, line height %d, baseline %d\n", f->width, f->height, f->line_height, f->baseline);
    if(glyph) {
      gfxboot_log("    bitmap size %dx%d\n", glyph->max_width, glyph->max_height);
    }
    gfxboot_log("    bitmap table: offset %u, size %u\n", f->bitmap.offset, f->bitmap.size);
    gfxboot_log("    char index: offset %u, size %u\n", f->unimap.offset, f->unimap.size);
    gfxboot_log("    data_id %s\n", gfx_obj_id2str(f->data_id));
    gfxboot_log("    glyph_id %s\n", gfx_obj_id2str(f->glyph_id));
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_font_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  font_t *font = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned more_gc = 0;

  if(font && data_size == OBJ_FONT_SIZE()) {
    more_gc += gfx_obj_ref_dec_delay_gc(font->parent_id);
    more_gc += gfx_obj_ref_dec_delay_gc(font->data_id);
    more_gc += gfx_obj_ref_dec_delay_gc(font->glyph_id);
  }

  return more_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_font_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  font_t *font = ptr->data.ptr;
  unsigned data_size = ptr->data.size;

  if(font && data_size == OBJ_FONT_SIZE()) {
    if(id == font->parent_id || id == font->data_id || id == font->glyph_id) return 1;
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_font_open(obj_id_t font_file)
{
  data_t *mem = gfx_obj_mem_ptr(font_file);
  unsigned ok = 0;

  if(!mem) return 0;

  obj_id_t font_id = gfx_obj_font_new();

  font_t *font = gfx_obj_font_ptr(font_id);

  if(!font) return 0;

  font->data_id = font_file;

  uint8_t *ptr = mem->ptr;
  unsigned size = mem->size;

  int max_bitmap_width = 0;
  int max_bitmap_height = 0;

  if(
    size >= 0x20 &&
    gfx_read_le32(ptr) == 0x864ab572
  ) {
    unsigned header_size = gfx_read_le32(ptr + 8);

    font->glyphs = gfx_read_le32(ptr + 16);
    font->glyph_size = gfx_read_le32(ptr + 20);
    font->height = (int) gfx_read_le32(ptr + 24);
    font->width = (int) gfx_read_le32(ptr + 28);
    font->line_height = font->height;

    font->bitmap.offset = header_size;
    font->bitmap.size = font->glyphs * font->glyph_size;

    font->unimap.offset = font->bitmap.offset + font->bitmap.size;
    font->unimap.size = size - font->unimap.offset;

    max_bitmap_width = font->width;
    max_bitmap_height = font->height;

    if(
      font->height == (int) font->glyph_size &&
      font->width <= 8 &&
      font->unimap.size > 0
    ) {
      font->type = 1;
      ok = 1;
    }
  }
  else if(
    size >= 0x10 &&
    gfx_read_le32(ptr) == 0xa42a9123
  ) {
    unsigned header_size = 0x10;

    font->glyphs = gfx_read_le32(ptr + 12);
    max_bitmap_width = ptr[7];
    max_bitmap_height = ptr[8];
    font->height = ptr[9];
    font->line_height = ptr[10];
    font->baseline = (int8_t) ptr[11];
    font->glyphs = gfx_read_le32(ptr + 12);

    font->unimap.offset = header_size;
    font->unimap.size = 5 * font->glyphs;

    font->bitmap.offset = font->unimap.offset + font->unimap.size;
    font->bitmap.size = size - font->bitmap.offset;

    if(font->bitmap.size > 0) {
      font->type = 2;
      ok = 1;
    }
  }

  if(ok) {
    font->glyph_id = gfx_obj_canvas_new(max_bitmap_width, max_bitmap_height);
    if(!font->glyph_id) ok = 0;
  }

  if(ok) {
    gfx_obj_ref_inc(font->data_id);
  }
  else {
    gfx_obj_ref_dec(font->glyph_id);
    gfx_obj_ref_dec(font_id);
    font_id = 0;
  }

  return font_id;
}
