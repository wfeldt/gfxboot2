#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// canvas

static char gfx_canvas_pixel2char(canvas_t *c, int x_blk, int y_blk, int x, int y);
static uint32_t gfx_canvas_chksum(canvas_t *c);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_canvas_new(int width, int height)
{
  obj_id_t id = gfx_obj_alloc(OTYPE_CANVAS, OBJ_CANVAS_SIZE(width, height));
  canvas_t *c = gfx_obj_canvas_ptr(id);

  if(c) {
    c->max_width = c->geo.width = width;
    c->max_height = c->geo.height = height;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
canvas_t *gfx_obj_canvas_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_CANVAS) return 0;

  return (canvas_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_canvas_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  canvas_t *c = ptr->data.ptr;
  unsigned len = ptr->data.size;
  int w, w_max, h, h_max, x_blk, y_blk;

  if(
    len != OBJ_CANVAS_SIZE(c->max_width, c->max_height) ||
    c->geo.width < 0 ||
    c->geo.height < 0 ||
    c->geo.width > c->max_width ||
    c->geo.height > c->max_height
  ) {
    if(style.ref) {
      gfxboot_log("      <invalid data>\n");
    }
    else {
      gfxboot_log("<invalid data>");
    }

    return 1;
  }

  len -= sizeof (canvas_t);

  x_blk = (c->geo.width + 79) / 80;
  if(!x_blk) x_blk = 1;
  w_max = c->geo.width / x_blk;

  y_blk = (c->geo.height + 19) / 20;
  if(!y_blk) y_blk = 1;
  h_max = c->geo.height / y_blk;

  if(!style.ref) {
    if(!style.inspect) return 0;

    gfxboot_log(
      "geo %dx%d_%dx%d, region %dx%d_%dx%d, chk 0x%08x",
      c->geo.x, c->geo.y, c->geo.width, c->geo.height,
      c->region.x, c->region.y, c->region.width, c->region.height,
      gfx_canvas_chksum(c)
    );

    return 1;
  }

  if(style.dump) {
    gfxboot_log("    cursor %dx%d_%dx%d, draw_mode %d, max %dx%d\n",
      c->cursor.x, c->cursor.y, c->cursor.width, c->cursor.height,
      c->draw_mode, c->max_width, c->max_height
    );
    gfxboot_log("    color #%08x, bg_color #%08x, font %s\n", c->color, c->bg_color, gfx_obj_id2str(c->font_id));
    gfxboot_log("    unit %dx%d\n", x_blk, y_blk);

    if(len) {
      for(h = 0; h < h_max; h++) {
        gfxboot_log("    |");
        for(w = 0; w < w_max; w++) {
          gfxboot_log("%c", gfx_canvas_pixel2char(c, x_blk, y_blk, w, h));
        }
        gfxboot_log("|\n");
      }
    }
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
char gfx_canvas_pixel2char(canvas_t *c, int x_blk, int y_blk, int x, int y)
{
  const char syms[] = " .,:+ox*%#O@";	// symbols for brightness
  color_t col, *cp = (color_t *) ((uint8_t *) c + sizeof (canvas_t));
  int i, j;
  unsigned val = 0;

  x *= x_blk;
  y *= y_blk;

  cp += x + y * c->geo.width;

  for(j = 0; j < y_blk; j++, cp += c->geo.width) {
    for(i = 0; i < x_blk; i++) {
      col = cp[i];
      unsigned alpha = 0xff - ((col >> 24) & 0xff);
      val += ((col & 0xff) + ((col >> 8) & 0xff) + ((col >> 16) & 0xff)) * alpha;
    }
  }

  val /= (unsigned) (x_blk * y_blk * 3 * 255 * 255) / (sizeof syms);	// yes, size + 1 !!!
  if(val > sizeof syms - 2) val = sizeof syms - 2;

  return syms[val];
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_canvas_adjust_size(canvas_t *c, int width, int height)
{
  if(width < 0 || height < 0 || width > c->max_width || height > c->max_height) return 0;

  c->geo.width = width;
  c->geo.height = height;

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_canvas_resize(obj_id_t canvas_id, int width, int height)
{
  canvas_t *c = gfx_obj_canvas_ptr(canvas_id);

  if(!c) return 0;

  if(c->geo.width == width  && c->geo.height == height) return 1;

  return gfx_obj_realloc(canvas_id, OBJ_CANVAS_SIZE(width, height)) ? 1 : 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint32_t gfx_canvas_chksum(canvas_t *c)
{
  unsigned u, len = (unsigned) c->geo.width * (unsigned) c->geo.height;
  uint32_t sum = 0, a = 0;

  for(u = 0; u < len; u++) {
    a = a * 73 + 19;
    sum += a ^ c->ptr[u];
  }

  return sum;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_canvas_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  canvas_t *canvas = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned more_gc = 0;

  if(canvas && data_size == OBJ_CANVAS_SIZE(canvas->max_width, canvas->max_height)) {
    more_gc += gfx_obj_ref_dec_delay_gc(canvas->font_id);
  }

  return more_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_canvas_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  canvas_t *canvas = ptr->data.ptr;
  unsigned data_size = ptr->data.size;

  if(canvas && data_size == OBJ_CANVAS_SIZE(canvas->max_width, canvas->max_height)) {
    if(id == canvas->font_id) return 1;
  }

  return 0;
}
