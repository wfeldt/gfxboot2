#include <gfxboot/gfxboot.h>

static uint8_t _console_font[4947];


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_init(int auto_run)
{
  gfxboot_log("gfxboot_init\n");

  gfxboot_log("data_t = %d, obj_t = %d\n", (int) sizeof (data_t), (int) sizeof (obj_t));

  if(gfx_malloc_init()) return 1;

  if(gfx_obj_init()) return 1;

  gfx_vm_status_dump();

  // memory debug example:
  //   gfxboot_data->vm.debug.show_pointer = 1;
  //   gfx_malloc_dump((dump_style_t) { .dump = 1, .no_check = 1, .max = 64 });
  //   gfx_obj_dump(OBJ_ID(0, 1), (dump_style_t) { .dump = 1, .no_check = 1, .max  = 64 });

  // setup compiled-in console font
  obj_id_t console_font_data_id = gfx_obj_const_mem_nofree_new(_console_font, sizeof _console_font, 0, 0);
  obj_id_t font_id = gfx_obj_font_open(console_font_data_id);
  // font structure itself holds ref to data
  gfx_obj_ref_dec(console_font_data_id);

  // create virtual screen, used for drawing
  gfxboot_data->screen.canvas_id = gfx_obj_canvas_new(gfxboot_data->screen.real.width, gfxboot_data->screen.real.height);
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->screen.canvas_id);
  if(!canvas) return 1;
  canvas->color = COLOR(0x00, 0xff, 0xff, 0xff);
  canvas->bg_color = COLOR(0xff, 0x00, 0x00, 0x00);

  // create default canvas ('background')
  gfxboot_data->canvas_id = gfx_obj_canvas_new(gfxboot_data->screen.real.width, gfxboot_data->screen.real.height);
  canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);
  if(!canvas) return 1;
  canvas->color = COLOR(0x00, 0xff, 0xff, 0xff);
  canvas->bg_color = COLOR(0xff, 0x00, 0x00, 0x00);

  // create canvas for debug console
  area_t area = gfx_font_dim(font_id);
  int t_width = area.width * 80;
  int t_height = area.height * 25;
  int t_x = (gfxboot_data->screen.real.width - t_width) / 2;
  int t_y = (gfxboot_data->screen.real.height - t_height) / 2;

  gfxboot_data->console.canvas_id = gfx_obj_canvas_new(t_width, t_height);
  canvas = gfx_obj_canvas_ptr(gfxboot_data->console.canvas_id);
  if(!canvas) return 1;
  canvas->draw_mode = dm_direct;
  canvas->font_id = font_id;
  canvas->geo.x = t_x;
  canvas->geo.y = t_y;
  canvas->cursor = (area_t) { .x = 0, .y = t_height - area.height, .width = area.width, .height = area.height };
  canvas->color = COLOR(0x00, 0xff, 0xff, 0xff);
  canvas->bg_color = COLOR(0x60, 0x32, 0x32, 0x32);
  gfx_rect(gfxboot_data->console.canvas_id, 0, 0, t_width, t_height, canvas->bg_color);

  // create default compose list and add default background canvas
  gfxboot_data->compose.list_id = gfx_obj_array_new(0);
  gfx_obj_array_push(gfxboot_data->compose.list_id, gfxboot_data->canvas_id, 1);

  if(!gfx_setup_dict()) return 1;

  // load main program
  obj_id_t pfile_id = gfx_read_file("main.gc");
  obj_t *pfile_ptr = gfx_obj_ptr(pfile_id);

  if(pfile_ptr) {
    pfile_ptr->flags.ro = 1;
  }
  else {
    gfxboot_log("no program to run - aborting\n");
    return 1;
  }

  if(!gfx_program_init(pfile_id)) {
    gfxboot_log("failed to setup program\n");
    return 1;
  }

  gfx_obj_ref_dec(pfile_id);

#if 0
  gfx_console_putc_xy(200, 100, L'€');
#endif

  // gfx_obj_asciiz_new("Foo Bar");

  // gfxboot_data->vm.debug.trace_ip = 1;

  if(auto_run) {
    gfxboot_data->vm.debug.console.show_on_error = 1;
    gfx_program_run();
  }
  gfx_show_error();

  // gfx_obj_dump(OBJ_ID(0, 1), (dump_style_t) { .dump = 1 });

  return 0;

  // return gfxboot_data->vm.error.id ? 1 : 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfxboot_debug_command(char *str)
{
  if(str) gfx_debug_cmd(str);
}


/*
  if ((action & 0x01)) menu_fini ();
  if ((action & 0x02)) *auto_boot = 1;
  if ((action & 0x04)) grub_cmdline_run (1);
  if ((action & 0x08)) goto refresh;
  if ((action & 0x10)) return action >> 8;
*/
int gfxboot_process_key(unsigned key)
{
  int action = 0;

  gfxboot_debug(2, 2, "gfxboot_process_key: key = 0x%08x\n", key);

  if(gfxboot_data->vm.debug.console.input) {
    gfx_program_debug(key);

    return action;
  }
  else {
    if(key == 0x04) {	// '^D'
      gfx_program_debug_on_off(1, 1);
      return action;
    }

    action = gfx_program_process_key(key);
  }

#if 0
  switch(key) {
    case '0'...'9':
      gfxboot_data->menu.entry = (int) key - '0';
      gfxboot_log("menu.entry = %d\n", gfxboot_data->menu.entry);
      break;

    case 'b':
      action = (gfxboot_data->menu.entry << 8) + 0x11;
      gfxboot_log("booting entry %d\n", gfxboot_data->menu.entry);
      break;

    case 'c':
      gfxboot_log("running cmdline\n");
      action = 0x04;
      break;

    case 'x':
      action = (-1 << 8) + 0x10 + 1;
      break;
  }
#endif

  gfxboot_debug(2, 2, "gfxboot_process_key: action = 0x%x\n", action);

  return action;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfxboot_timeout()
{
  gfxboot_log(
    "gfxboot_timeout: max = %d, current = %d\n",
    gfxboot_data->menu.timeout.max,
    gfxboot_data->menu.timeout.current
  );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// lat9v-16.psfu
//
static uint8_t _console_font[4947] = {
  0x72, 0xb5, 0x4a, 0x86, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0xc3, 0x99, 0x99, 0xf3, 0xe7, 0xe7, 0xff, 0xe7, 0xe7, 0x7e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x6e, 0xf8, 0xd8, 0xd8, 0xdc, 0xd8, 0xd8, 0xd8, 0xf8, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0xdb, 0xdb, 0xdf, 0xd8, 0xdb, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf8, 0x80, 0xe0, 0x80, 0x80, 0x00, 0x3e, 0x20, 0x38, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x70, 0x88, 0x80, 0x88, 0x70, 0x00, 0x3c, 0x22, 0x3c, 0x24, 0x22, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x80, 0x80, 0x80, 0xf8, 0x00, 0x3e, 0x20, 0x38, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44,
  0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
  0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x00, 0x88, 0xc8, 0xa8, 0x98, 0x88, 0x00, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x88, 0x88, 0x50, 0x50, 0x20, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0e, 0x38, 0xe0, 0x38, 0x0e, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xe0, 0x38, 0x0e, 0x38, 0xe0, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x06, 0x0c, 0xfe, 0x18, 0x30, 0xfe, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x06, 0x1e, 0x7e, 0xfe, 0x7e, 0x1e, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xc0, 0xf0, 0xfc, 0xfe, 0xfc, 0xf0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x6c, 0xfe, 0x6c, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x06, 0x36, 0x66, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x10, 0x10, 0x7c, 0xd6, 0xd0, 0xd0, 0x7c, 0x16, 0x16, 0xd6, 0x7c, 0x10, 0x10, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xc2, 0xc6, 0x0c, 0x18, 0x30, 0x60, 0xc6, 0x86, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0x06, 0x06, 0x3c, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x0c, 0x1e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xc6, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x06, 0x0c, 0x78, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x0c, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xde, 0xde, 0xde, 0xdc, 0xc0, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0x66, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xde, 0xc6, 0xc6, 0x66, 0x3a, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0xcc, 0x78, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xe6, 0x66, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x0c, 0x0e, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x64, 0x38, 0x0c, 0x06, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0x7e, 0x5a, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0xfe, 0xee, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0x6c, 0x7c, 0x38, 0x38, 0x7c, 0x6c, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xc6, 0x86, 0x0c, 0x18, 0x30, 0x60, 0xc2, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
  0x00, 0x30, 0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xe0, 0x60, 0x60, 0x78, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1c, 0x0c, 0x0c, 0x3c, 0x6c, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x6c, 0x64, 0x60, 0xf0, 0x60, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xcc, 0x78, 0x00,
  0x00, 0x00, 0xe0, 0x60, 0x60, 0x6c, 0x76, 0x66, 0x66, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x06, 0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00,
  0x00, 0x00, 0xe0, 0x60, 0x60, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xec, 0xfe, 0xd6, 0xd6, 0xd6, 0xd6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0x0c, 0x1e, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0xfe, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0xf8, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xcc, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0e, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x60, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x60, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x60, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x60, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x0c, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0xec, 0x0c, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0xef, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x0c, 0xec, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0xec, 0x0c, 0xec, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xef, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0xef, 0x00, 0xef, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x10, 0x7c, 0xd6, 0xd0, 0xd0, 0xd0, 0xd6, 0x7c, 0x10, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x6c, 0x60, 0x60, 0xf0, 0x60, 0x60, 0x66, 0xf6, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x1c, 0x32, 0x60, 0x60, 0xfc, 0x60, 0xfc, 0x60, 0x60, 0x32, 0x1c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x38, 0x00, 0x7c, 0xc6, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x7c, 0xc6, 0x60, 0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x38, 0x0c, 0xc6, 0x7c, 0x00, 0x00, 0x00,
  0x00, 0x6c, 0x38, 0x00, 0x00, 0x7c, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x42, 0x99, 0xa5, 0xa1, 0xa5, 0x99, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x6c, 0x6c, 0x3e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x6c, 0xd8, 0x6c, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0xa5, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x38, 0x6c, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
  0x38, 0x6c, 0x18, 0x30, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x38, 0x6c, 0x18, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x38, 0x00, 0xfe, 0xc6, 0x8c, 0x18, 0x30, 0x60, 0xc2, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xf6, 0xc0, 0xc0, 0xc0, 0x00,
  0x00, 0x00, 0x7f, 0xd6, 0xd6, 0x76, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x6c, 0x38, 0x00, 0xfe, 0xcc, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x30, 0x70, 0x30, 0x30, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x6c, 0x36, 0x6c, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x77, 0xcc, 0xcc, 0xcc, 0xcf, 0xcf, 0xcc, 0xcc, 0xcc, 0x77, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0xdb, 0xdb, 0xdf, 0xd8, 0xdb, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x30, 0x60, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
  0x60, 0x30, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x18, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x38, 0x6c, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x76, 0xdc, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x6c, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x38, 0x6c, 0x38, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3e, 0x78, 0xd8, 0xd8, 0xfc, 0xd8, 0xd8, 0xd8, 0xd8, 0xde, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x0c, 0x66, 0x3c, 0x00,
  0x60, 0x30, 0x00, 0xfe, 0x66, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x18, 0x00, 0xfe, 0x66, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x38, 0x6c, 0x00, 0xfe, 0x66, 0x60, 0x7c, 0x60, 0x60, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x6c, 0x00, 0xfe, 0x66, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x60, 0x30, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x06, 0x0c, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x3c, 0x66, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x66, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0x6c, 0x66, 0x66, 0xf6, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x76, 0xdc, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x60, 0x30, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x76, 0xdc, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0xc6, 0xce, 0xce, 0xde, 0xf6, 0xe6, 0xe6, 0xc6, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x60, 0x30, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x38, 0x6c, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x6c, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x06, 0x0c, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf0, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xcc, 0xc6, 0xc6, 0xc6, 0xd6, 0xdc, 0x80, 0x00, 0x00, 0x00,
  0x00, 0x60, 0x30, 0x18, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x18, 0x30, 0x60, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x10, 0x38, 0x6c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x38, 0x6c, 0x38, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xdb, 0x1b, 0x7f, 0xd8, 0xdb, 0x7e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x18, 0x6c, 0x38, 0x00,
  0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x0c, 0x18, 0x30, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x60, 0x30, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x0c, 0x18, 0x30, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x18, 0x3c, 0x66, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x78, 0x30, 0x78, 0x0c, 0x7e, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x76, 0xdc, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x0c, 0x18, 0x30, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xce, 0xde, 0xfe, 0xf6, 0xe6, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x60, 0x30, 0x18, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x18, 0x30, 0x60, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x30, 0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x0c, 0x18, 0x30, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0xf8, 0x00,
  0x00, 0x00, 0xf0, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0xf8, 0x00,
  0xef, 0xbf, 0xbd, 0xff, 0xe2, 0x89, 0x88, 0xff, 0xc5, 0x92, 0xff, 0xc5, 0x93, 0xff, 0xe2, 0x97,
  0x86, 0xff, 0xe2, 0x90, 0x89, 0xff, 0xe2, 0x90, 0x8c, 0xff, 0xe2, 0x90, 0x8d, 0xff, 0xe2, 0x90,
  0x8a, 0xff, 0xe2, 0x96, 0x91, 0xff, 0xe2, 0x96, 0x92, 0xff, 0xe2, 0x96, 0x93, 0xff, 0xe2, 0x96,
  0x88, 0xff, 0xe2, 0x96, 0x84, 0xff, 0xe2, 0x96, 0x80, 0xff, 0xe2, 0x96, 0x8c, 0xff, 0xe2, 0x96,
  0x90, 0xff, 0xe2, 0x90, 0xa4, 0xff, 0xe2, 0x90, 0x8b, 0xff, 0xe2, 0x89, 0xa4, 0xff, 0xe2, 0x89,
  0xa5, 0xff, 0xe2, 0x89, 0xa0, 0xff, 0xe2, 0x97, 0x80, 0xff, 0xe2, 0x96, 0xb6, 0xff, 0xe2, 0x86,
  0x91, 0xff, 0xe2, 0x86, 0x93, 0xff, 0xe2, 0x86, 0x92, 0xff, 0xe2, 0x86, 0x90, 0xff, 0xe2, 0x86,
  0x95, 0xff, 0xe2, 0x86, 0x94, 0xff, 0xe2, 0x86, 0xb5, 0xff, 0xcf, 0x80, 0xff, 0x20, 0xc2, 0xa0,
  0xe2, 0x80, 0x80, 0xe2, 0x80, 0x81, 0xe2, 0x80, 0x82, 0xe2, 0x80, 0x83, 0xe2, 0x80, 0x84, 0xe2,
  0x80, 0x85, 0xe2, 0x80, 0x86, 0xe2, 0x80, 0x87, 0xe2, 0x80, 0x88, 0xe2, 0x80, 0x89, 0xe2, 0x80,
  0x8a, 0xe2, 0x80, 0xaf, 0xff, 0x21, 0xff, 0x22, 0xff, 0x23, 0xff, 0x24, 0xff, 0x25, 0xff, 0x26,
  0xff, 0x27, 0xff, 0x28, 0xff, 0x29, 0xff, 0x2a, 0xff, 0x2b, 0xff, 0x2c, 0xff, 0x2d, 0xff, 0x2e,
  0xff, 0x2f, 0xff, 0x30, 0xff, 0x31, 0xff, 0x32, 0xff, 0x33, 0xff, 0x34, 0xff, 0x35, 0xff, 0x36,
  0xff, 0x37, 0xff, 0x38, 0xff, 0x39, 0xff, 0x3a, 0xff, 0x3b, 0xff, 0x3c, 0xff, 0x3d, 0xff, 0x3e,
  0xff, 0x3f, 0xff, 0x40, 0xff, 0x41, 0xff, 0x42, 0xff, 0x43, 0xff, 0x44, 0xff, 0x45, 0xff, 0x46,
  0xff, 0x47, 0xff, 0x48, 0xff, 0x49, 0xff, 0x4a, 0xff, 0x4b, 0xe2, 0x84, 0xaa, 0xff, 0x4c, 0xff,
  0x4d, 0xff, 0x4e, 0xff, 0x4f, 0xff, 0x50, 0xff, 0x51, 0xff, 0x52, 0xff, 0x53, 0xff, 0x54, 0xff,
  0x55, 0xff, 0x56, 0xff, 0x57, 0xff, 0x58, 0xff, 0x59, 0xff, 0x5a, 0xff, 0x5b, 0xff, 0x5c, 0xff,
  0x5d, 0xff, 0x5e, 0xff, 0x5f, 0xef, 0xa0, 0x84, 0xff, 0x60, 0xff, 0x61, 0xff, 0x62, 0xff, 0x63,
  0xff, 0x64, 0xff, 0x65, 0xff, 0x66, 0xff, 0x67, 0xff, 0x68, 0xff, 0x69, 0xff, 0x6a, 0xff, 0x6b,
  0xff, 0x6c, 0xff, 0x6d, 0xff, 0x6e, 0xff, 0x6f, 0xff, 0x70, 0xff, 0x71, 0xff, 0x72, 0xff, 0x73,
  0xff, 0x74, 0xff, 0x75, 0xff, 0x76, 0xff, 0x77, 0xff, 0x78, 0xff, 0x79, 0xff, 0x7a, 0xff, 0x7b,
  0xff, 0x7c, 0xff, 0x7d, 0xff, 0x7e, 0xff, 0xc5, 0xb8, 0xff, 0xef, 0xa0, 0x81, 0xff, 0xe2, 0x95,
  0xb5, 0xff, 0xe2, 0x95, 0xb6, 0xff, 0xe2, 0x94, 0x94, 0xff, 0xe2, 0x95, 0xb7, 0xff, 0xe2, 0x94,
  0x82, 0xff, 0xe2, 0x94, 0x8c, 0xff, 0xe2, 0x94, 0x9c, 0xff, 0xe2, 0x95, 0xb4, 0xff, 0xe2, 0x94,
  0x98, 0xff, 0xe2, 0x94, 0x80, 0xff, 0xe2, 0x94, 0xb4, 0xff, 0xe2, 0x94, 0x90, 0xff, 0xe2, 0x94,
  0xa4, 0xff, 0xe2, 0x94, 0xac, 0xff, 0xe2, 0x94, 0xbc, 0xff, 0xef, 0xa0, 0x83, 0xff, 0xe2, 0x95,
  0xb9, 0xff, 0xe2, 0x95, 0xba, 0xff, 0xe2, 0x94, 0x97, 0xe2, 0x95, 0x9a, 0xff, 0xe2, 0x95, 0xbb,
  0xff, 0xe2, 0x94, 0x83, 0xe2, 0x95, 0x91, 0xff, 0xe2, 0x94, 0x8f, 0xe2, 0x95, 0x94, 0xff, 0xe2,
  0x94, 0xa3, 0xe2, 0x95, 0xa0, 0xff, 0xe2, 0x95, 0xb8, 0xff, 0xe2, 0x94, 0x9b, 0xe2, 0x95, 0x9d,
  0xff, 0xe2, 0x94, 0x81, 0xe2, 0x95, 0x90, 0xff, 0xe2, 0x94, 0xbb, 0xe2, 0x95, 0xa9, 0xff, 0xe2,
  0x94, 0x93, 0xe2, 0x95, 0x97, 0xff, 0xe2, 0x95, 0xa3, 0xe2, 0x94, 0xab, 0xff, 0xe2, 0x94, 0xb3,
  0xe2, 0x95, 0xa6, 0xff, 0xe2, 0x95, 0x8b, 0xe2, 0x95, 0xac, 0xff, 0xe2, 0x90, 0xa3, 0xff, 0xc2,
  0xa1, 0xff, 0xc2, 0xa2, 0xff, 0xc2, 0xa3, 0xff, 0xe2, 0x82, 0xac, 0xff, 0xc2, 0xa5, 0xff, 0xc5,
  0xa0, 0xff, 0xc2, 0xa7, 0xff, 0xc5, 0xa1, 0xff, 0xc2, 0xa9, 0xff, 0xc2, 0xaa, 0xff, 0xc2, 0xab,
  0xff, 0xc2, 0xac, 0xff, 0xc2, 0xad, 0xff, 0xc2, 0xae, 0xff, 0xc2, 0xaf, 0xef, 0xa0, 0x80, 0xff,
  0xc2, 0xb0, 0xff, 0xc2, 0xb1, 0xff, 0xc2, 0xb2, 0xff, 0xc2, 0xb3, 0xff, 0xc5, 0xbd, 0xff, 0xc2,
  0xb5, 0xff, 0xc2, 0xb6, 0xff, 0xc2, 0xb7, 0xff, 0xc5, 0xbe, 0xff, 0xc2, 0xb9, 0xff, 0xc2, 0xba,
  0xff, 0xc2, 0xbb, 0xff, 0xc5, 0x92, 0xff, 0xc5, 0x93, 0xff, 0xc5, 0xb8, 0xff, 0xc2, 0xbf, 0xff,
  0xc3, 0x80, 0xff, 0xc3, 0x81, 0xff, 0xc3, 0x82, 0xff, 0xc3, 0x83, 0xff, 0xc3, 0x84, 0xff, 0xc3,
  0x85, 0xe2, 0x84, 0xab, 0xff, 0xc3, 0x86, 0xff, 0xc3, 0x87, 0xff, 0xc3, 0x88, 0xff, 0xc3, 0x89,
  0xff, 0xc3, 0x8a, 0xff, 0xc3, 0x8b, 0xff, 0xc3, 0x8c, 0xff, 0xc3, 0x8d, 0xff, 0xc3, 0x8e, 0xff,
  0xc3, 0x8f, 0xff, 0xc3, 0x90, 0xff, 0xc3, 0x91, 0xff, 0xc3, 0x92, 0xff, 0xc3, 0x93, 0xff, 0xc3,
  0x94, 0xff, 0xc3, 0x95, 0xff, 0xc3, 0x96, 0xff, 0xc3, 0x97, 0xff, 0xc3, 0x98, 0xff, 0xc3, 0x99,
  0xff, 0xc3, 0x9a, 0xff, 0xc3, 0x9b, 0xff, 0xc3, 0x9c, 0xff, 0xc3, 0x9d, 0xff, 0xc3, 0x9e, 0xff,
  0xc3, 0x9f, 0xff, 0xc3, 0xa0, 0xff, 0xc3, 0xa1, 0xff, 0xc3, 0xa2, 0xff, 0xc3, 0xa3, 0xff, 0xc3,
  0xa4, 0xff, 0xc3, 0xa5, 0xff, 0xc3, 0xa6, 0xff, 0xc3, 0xa7, 0xff, 0xc3, 0xa8, 0xff, 0xc3, 0xa9,
  0xff, 0xc3, 0xaa, 0xff, 0xc3, 0xab, 0xff, 0xc3, 0xac, 0xff, 0xc3, 0xad, 0xff, 0xc3, 0xae, 0xff,
  0xc3, 0xaf, 0xff, 0xc3, 0xb0, 0xff, 0xc3, 0xb1, 0xff, 0xc3, 0xb2, 0xff, 0xc3, 0xb3, 0xff, 0xc3,
  0xb4, 0xff, 0xc3, 0xb5, 0xff, 0xc3, 0xb6, 0xff, 0xc3, 0xb7, 0xff, 0xc3, 0xb8, 0xff, 0xc3, 0xb9,
  0xff, 0xc3, 0xba, 0xff, 0xc3, 0xbb, 0xff, 0xc3, 0xbc, 0xff, 0xc3, 0xbd, 0xff, 0xc3, 0xbe, 0xff,
  0xc3, 0xbf, 0xff
};
