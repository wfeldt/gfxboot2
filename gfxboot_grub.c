#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/command.h>
#include <grub/video.h>
#include <grub/gfxterm.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/normal.h>
#include <grub/menu.h>
#include <grub/menu_viewer.h>
#include <grub/time.h>
#include <grub/cpu/io.h>

#include <gfxboot/gfxboot.h>

GRUB_MOD_LICENSE ("GPLv3+");


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
gfxboot_data_t *gfxboot_data;


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static void serial_init(void);
static void serial_putc(int key);

static void grub_gfxboot_page_0(void);
static grub_err_t grub_gfxboot_init(int entry, grub_menu_t menu, int nested);
static int grub_gfxboot_process_key(int *key);
static void grub_gfxboot_set_chosen_entry(int entry, void *data);
static void grub_gfxboot_print_timeout(int timeout, void *data);
static void grub_gfxboot_clear_timeout(void *data);
static void grub_gfxboot_data_free(void);
static void grub_gfxboot_fini(void *data);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void serial_init()
{
  int x;
  unsigned port = 0x3f8;

  gfxboot_data->serial.port = port;

  // DLAB = 1
  grub_outb(0x83, port + 3);

  // set to 115200 baud
  grub_outb(1, port + 0);
  grub_outb(0, port + 1);

  // DLAB = 0, 8 bits, no parity
  grub_outb(0x03, port + 3);

  x = grub_inb(port + 3);

  // gfxboot_log("x = 0x%02x\n", x);

  if(x == 0xff) {
    gfxboot_data->serial.port = 0;
    return;
  }

  // IRQ disable
  grub_outb(0x00, port + 1);

  // FIFO enable
  grub_outb(0x01, port + 2);

  // set terminal colors to normal, looks better
  gfxboot_log("\033[00m");
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void serial_putc(int key)
{
  unsigned cnt = 1 << 20;

  if(!gfxboot_data || !gfxboot_data->serial.port) return;

  // wait until ready, but not indefinitely...
  while(!(grub_inb(gfxboot_data->serial.port + 5) & 0x20) && cnt--);

  grub_outb(key, gfxboot_data->serial.port);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Print to serial line and/or console.
//
// dst - bitmask; bit 0: serial, bit 1: console
//
int gfxboot_printf(int dst, const char *format, ...)
{
  char *s;
  int log_level_serial = (signed char) ((dst >> 8) & 0xff);
  int log_level_console = (signed char) ((dst >> 16) & 0xff);

  if(!format) return 0;

  va_list args;
  va_start(args, format);
  s = grub_xvasprintf(format, args);
  va_end(args);

  if(
    (dst & 1) &&
    gfxboot_data &&
    log_level_serial <= gfxboot_data->vm.debug.log_level_serial
  ) {
    char *t = s;
    while(*t) {
      if(*t == '\n') serial_putc('\r');
      serial_putc(*t++);
    }
  }

  if(
    (dst & 2) &&
    gfxboot_data &&
    log_level_console <= gfxboot_data->vm.debug.log_level_console
  ) {
    char *t = s;
    while(*t) {
      if(*t == '\n') gfx_console_putc('\r', 1);
      gfx_console_putc(*t++, 1);
    }
  }

  grub_free(s);

  return grub_strlen(s);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Print to buffer.
//
int gfxboot_asprintf(char **str, const char *format, ...)
{
  char *s;

  va_list args;
  va_start(args, format);
  *str = s = grub_xvasprintf(format, args);
  va_end(args);

  return s ? (int) grub_strlen(s) : -1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Print to static buffer.
//
int gfxboot_snprintf(char *str, unsigned size, const char *format, ...)
{
  int len;

  va_list args;
  va_start(args, format);
  len = grub_vsnprintf(str, size, format, args);
  va_end(args);

  return len;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void grub_gfxboot_page_0()
{
  void *fb;

  grub_video_get_raw_info(NULL, &fb);

  if(fb != gfxboot_data->screen.real.ptr) grub_video_swap_buffers();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
grub_err_t grub_gfxboot_init(int entry, grub_menu_t menu, int nested)
{
  struct grub_video_mode_info mode_info;
  struct grub_menu_viewer *instance;
  grub_err_t err = GRUB_ERR_NONE;
  void *fb0 = NULL, *fb1 = NULL;

  int i;

  grub_gfxboot_data_free();

  gfxboot_data = grub_zalloc(sizeof *gfxboot_data);
  if(!gfxboot_data) return grub_errno;

  instance = grub_zalloc(sizeof *instance);
  if(!instance) return grub_errno;

  // don't log to console
  gfxboot_data->vm.debug.log_level_console = -1;

  gfxboot_data->menu.entry = entry;
  gfxboot_data->menu.nested = nested;
  gfxboot_data->menu.timeout.max = grub_menu_get_timeout();

  if(menu->size > 0) {
    grub_menu_entry_t me;

    gfxboot_data->menu.size = menu->size;
    gfxboot_data->menu.entries = grub_zalloc(menu->size * sizeof *gfxboot_data->menu.entries);
    if(!gfxboot_data->menu.entries) return grub_errno;

    for(i = 0, me = menu->entry_list; me && i < menu->size; me = me->next, i++) {
      gfxboot_data->menu.entries[i].title = me->title;
    }
  }

  err = grub_video_get_raw_info(&mode_info, &fb0);

  if(err) return err;

  grub_video_swap_buffers();

  grub_video_get_raw_info(NULL, &fb1);

  if(fb1 == NULL || fb0 <= fb1) {
    grub_video_swap_buffers();
    gfxboot_data->screen.real.ptr = fb0;
  }
  else {
    gfxboot_data->screen.real.ptr = fb1;
  }

  if(!gfxboot_data->screen.real.ptr) return GRUB_ERR_BAD_DEVICE;

  gfxboot_data->screen.real.width = mode_info.width;
  gfxboot_data->screen.real.height = mode_info.height;
  gfxboot_data->screen.real.bytes_per_line = mode_info.pitch;
  gfxboot_data->screen.real.bytes_per_pixel = mode_info.bytes_per_pixel;
  gfxboot_data->screen.real.bits_per_pixel = mode_info.bpp;

  gfxboot_data->screen.real.red.pos = mode_info.red_field_pos;
  gfxboot_data->screen.real.red.size = mode_info.red_mask_size;
  gfxboot_data->screen.real.green.pos = mode_info.green_field_pos;
  gfxboot_data->screen.real.green.size = mode_info.green_mask_size;
  gfxboot_data->screen.real.blue.pos = mode_info.blue_field_pos;
  gfxboot_data->screen.real.blue.size = mode_info.blue_mask_size;
  gfxboot_data->screen.real.res.pos = mode_info.reserved_field_pos;
  gfxboot_data->screen.real.res.size = mode_info.reserved_mask_size;

  grub_video_set_viewport(0, 0, gfxboot_data->screen.real.width, gfxboot_data->screen.real.height);

#if 0
  // setup a commandline terminal window
  // cf. gfxmenu/init_terminal()
  grub_gfxterm_set_window(
    GRUB_VIDEO_RENDER_TARGET_DISPLAY,
    100, 100,
    300, 200,
    0,
    grub_font_get(""),
    0
  );
#endif

  instance->data = NULL;
  instance->set_chosen_entry = grub_gfxboot_set_chosen_entry;
  instance->print_timeout = grub_gfxboot_print_timeout;
  instance->clear_timeout = grub_gfxboot_clear_timeout;
  instance->process_key = grub_gfxboot_process_key;
  instance->fini = grub_gfxboot_fini;

  serial_init();

  // reserve 16 MiB for our VM
  gfxboot_data->vm.mem.size = 16 * (1 << 20);
  gfxboot_data->vm.mem.ptr = grub_zalloc(gfxboot_data->vm.mem.size);
  if(!gfxboot_data->vm.mem.ptr) return grub_errno;

  if(grub_video_adapter_active) {
    gfxboot_log("adapter name: %s\n", grub_video_adapter_active->name);
  }

  gfxboot_log(
    "video mode info:\n  type = 0x%x\n  fb0 = %p, fb1 = %p\n",
    mode_info.mode_type, fb0, fb1
  );

  if(gfxboot_init()) {
    err = GRUB_ERR_MENU;
  }
  else {
    grub_menu_register_viewer(instance);
  }

  return err;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int grub_gfxboot_process_key(int *key)
{
  int action;
  int key2;

  grub_gfxboot_page_0();

  if(!gfxboot_data || !key) return 0;

  key2 = *key;

  if((key2 & GRUB_TERM_CTRL)) {
    key2 &= 0x1f;
  }

  gfxboot_debug(2, 2, "grub_gfxboot_process_key: key = 0x%x '%c'\n", key2, key2 >= ' ' ? key2 : ' ');

  if(gfxboot_data->menu.timeout.current > 0) {
    grub_env_unset("timeout");
    grub_env_unset("fallback");
    grub_gfxboot_clear_timeout(NULL);
  }

  action = gfxboot_process_key(key2);

  gfxboot_debug(1, 2, "grub_gfxboot_process_key: action = %d.0x%02x\n", action >> 8, action & 0xff);

  *key = 0;

  return action;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void grub_gfxboot_set_chosen_entry(int entry, void *data __attribute__ ((unused)))
{
  gfxboot_data->menu.entry = entry;

  gfxboot_log("set_chosen_entry: %d\n", entry);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void grub_gfxboot_print_timeout(int timeout, void *data __attribute__ ((unused)))
{
  gfxboot_data->menu.timeout.current = timeout;

  gfxboot_timeout();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void grub_gfxboot_clear_timeout(void *data __attribute__ ((unused)))
{
  gfxboot_data->menu.timeout.current = 0;

  gfxboot_timeout();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_sys_read_file(char *name, void **buf)
{
  int size = -1;
  char *tmp = NULL;

  *buf = NULL;

  grub_errno = 0;

  if(!name) return size;

  if(*name != '/' && *name != '(') {
    const char *prefix = grub_env_get("prefix");
    if(!prefix) prefix = "";

    int prefix_len = grub_strlen(prefix);
    int name_len = grub_strlen(name);

    tmp = grub_zalloc(prefix_len + name_len + 2);

    if(!tmp) return size;

    gfx_memcpy(tmp, prefix, prefix_len);
    gfx_memcpy(tmp + prefix_len, "/", 1);
    gfx_memcpy(tmp + prefix_len + 1, name, name_len);

    name = tmp;
  }

  grub_file_t file = grub_file_open(name, GRUB_FILE_TYPE_NONE);

  gfxboot_log("open(%s) = %p\n", name, file);

  if(file) {
    if(file->size == 0) {
      size = 0;
    }
    else if(file->size > 0) {
      *buf = grub_zalloc(file->size);
      if(*buf) {
        size = grub_file_read(file, *buf, file->size);
        if(size != (int) file->size) {
          grub_free(*buf);
          *buf = NULL;
          size = -1;
        }
      }
    }

    grub_file_close(file);
  }

  gfxboot_log("read(%s) = %d\n", name, size);

  grub_free(tmp);

  return size;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfxboot_sys_free(void *ptr)
{
  if(ptr) grub_free(ptr);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned long gfxboot_sys_strlen(const char *s)
{
  return grub_strlen(s);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_sys_strcmp(const char *s1, const char *s2)
{
  return grub_strcmp(s1, s2);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
long int gfxboot_sys_strtol(const char *nptr, char **endptr, int base)
{
  return grub_strtol(nptr, endptr, base);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfxboot_screen_update(area_t area __attribute__ ((unused)))
{
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_getkey()
{
  return grub_getkey();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void grub_gfxboot_data_free()
{
  if(gfxboot_data) {
    grub_free(gfxboot_data->menu.entries);
    grub_free(gfxboot_data->vm.mem.ptr);

    grub_free(gfxboot_data);
  }

  gfxboot_data = NULL;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void grub_gfxboot_fini(void *data __attribute__ ((unused)))
{
  gfxboot_log("grub_gfxboot_fini %p\n", gfxboot_data);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
GRUB_MOD_INIT(gfxboot)
{
  struct grub_term_output *term;

  FOR_ACTIVE_TERM_OUTPUTS(term) {
    if(grub_gfxmenu_try_hook && term->fullscreen) {
      term->fullscreen();
      break;
    }
  }

  grub_gfxmenu_try_hook = grub_gfxboot_init;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
GRUB_MOD_FINI(gfxboot)
{
  gfxboot_log("gfxboot fini %p\n", gfxboot_data);

  grub_gfxboot_data_free();

  grub_gfxmenu_try_hook = NULL;
}

