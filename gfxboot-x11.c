#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <getopt.h>

#include <gfxboot.h>

#define FAKE_WIDTH	800
#define FAKE_HEIGHT	600

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
gfxboot_data_t *gfxboot_data;

struct {
  unsigned width, height;
  Display *display;
  Window window;
  GC gc;
  XIM xim;
  XIC xic;
  XImage *ximage;
  Visual *visual;
  int depth;
  int timeout;
} config;

XImage fake_ximage = {
 .width = FAKE_WIDTH, .height = FAKE_HEIGHT,
 .bitmap_pad = 32, .bytes_per_line = FAKE_WIDTH * 4, .bits_per_pixel = 32,
 .red_mask = 0xff0000, .green_mask = 0xff00, .blue_mask = 0xff
};

struct {
  unsigned verbose:1;
  unsigned help:1;
  unsigned x11:1;
  FILE *debug_file;
} opt;

struct option options[] = {
  { "help", 0, NULL, 'h' },
  { "verbose", 0, NULL, 'v' },
  { "no-x11", 0, NULL, 'n' },
  { "file", 1, NULL, 1 },
  { }
};


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void help(void);
void run_debug_commands(FILE *file);

int NewXErrorHandler(Display *display, XErrorEvent *xev);
int NewXIOErrorHandler(Display *display);
color_bits_t mask_to_color_bits(unsigned mask);

int x11_create_window(unsigned width, unsigned height);
int x11_close_window(void);
int x11_event_loop(void);

int x11_gfxboot_init(void);
int x11_gfxboot_process_key(int *key);
void x11_gfxboot_print_timeout(int timeout, void *data);
void x11_gfxboot_clear_timeout(void *data);
void x11_gfxboot_data_free(void);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int main(int argc, char **argv)
{
  int i;

  opterr = 0;

  opt.x11 = 1;

  while((i = getopt_long(argc, argv, "hv", options, NULL)) != -1) {
    switch(i) {
      case 1:
        if(!strcmp(optarg, "-")) {
          opt.debug_file = stdin;
        }
        else {
          opt.debug_file = fopen(optarg, "r");
        }
        break;

      case 'v':
        opt.verbose = 1;
        break;

      case 'n':
        opt.x11 = 0;
        break;

      default:
        help();
        return i == 'h' ? 0 : 1;
    }
  }

  if(argc == optind + 1) {
    if(chdir(argv[optind])) {
      perror(argv[optind]);
      return 1;
    }
  }
  else {
    fprintf(stderr, "gfxboot-x11: directory missing\n");
    help();
    return 1;
  }

  if(opt.x11) {
    XSetErrorHandler(NewXErrorHandler);
    XSetIOErrorHandler(NewXIOErrorHandler);

    if(x11_create_window(FAKE_WIDTH, FAKE_HEIGHT)) return 1;
  }
  else {
    config.ximage = &fake_ximage;
    config.ximage->data = calloc(1, (size_t) (config.ximage->bytes_per_line * config.ximage->height));
    config.width = (unsigned) config.ximage->width;
    config.height = (unsigned) config.ximage->height;
    config.depth = config.ximage->bits_per_pixel;
  }

  config.timeout = opt.debug_file ? 0 : 10;

  if(x11_gfxboot_init()) return 2;

  run_debug_commands(opt.debug_file);

  if(opt.x11) {
    x11_gfxboot_print_timeout(config.timeout, NULL);
    x11_event_loop();
    x11_close_window();
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Display short usage message.
void help()
{
  printf(
    "Usage: gfxboot-x11 [OPTIONS] DIR\n"
    "\n"
    "Run gfxboot file in DIR.\n"
   "\n"
    "Options:\n"
    "      --file FILE       Run debug commands from FILE at startup.\n"
    "  -v, --verbose         Show more detailed info.\n"
    "  -h, --help            Show this text.\n"
    "\n"
  );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void run_debug_commands(FILE *file)
{
  if(!file) return;

  char *buf = NULL;
  size_t len = 0;
  ssize_t line_len;

  while((line_len = getline(&buf, &len, file)) > 0) {
    buf[line_len - 1] = 0;	// strip newline
    gfxboot_debug_command(buf);
  }

  fclose(file);

  free(buf);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int NewXErrorHandler(Display *display, XErrorEvent *xev)
{
  gfxboot_log("X Error\n");

  exit(1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int NewXIOErrorHandler(Display *display)
{
  // gfxboot_log("X IO Error\n");

  exit(0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
color_bits_t mask_to_color_bits(unsigned mask)
{
  color_bits_t x = { 0, 0 };

  if(mask) for(; !(mask & 1); mask >>= 1, x.pos++);
  if(mask) for(; mask; mask >>= 1, x.size++);

  return x;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int x11_create_window(unsigned width, unsigned height)
{
  char *title = "gfxboot2";

  config.width = width;
  config.height = height;

  XSizeHints hints;

  hints.width_inc = 1;
  hints.height_inc = 1;

  hints.min_width = hints.max_width = hints.width = (int) config.width;
  hints.min_height = hints.max_height = hints.height = (int) config.height;

  hints.flags = PResizeInc | PSize | PMinSize | PMaxSize;

  if((config.display = XOpenDisplay(NULL)) == NULL) {
    gfxboot_log("Error: failed to open display.\n");

    return 1;
  }

  config.window = XCreateSimpleWindow(
    config.display, DefaultRootWindow(config.display),
    0, 0,
    config.width, config.height,
    0, 0, 0
  );

  gfxboot_log("window id: 0x%08x\n", (unsigned) config.window);

  if(!config.window) {
    gfxboot_log("Error: X server does not co-operate\n");
    XCloseDisplay(config.display);

    return 2;
  }

  config.xim = XOpenIM(config.display, NULL, NULL, NULL);
  if(config.xim) {
    config.xic = XCreateIC(config.xim,
      XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
      XNClientWindow, config.window,
      XNFocusWindow, config.window,
      NULL
    );
  }

  fprintf(stderr, "xim = %p, xic = %p\n", config.xim, config.xic);

  XSetWindowAttributes xswa;

  xswa.backing_store = Always;
  xswa.backing_planes = 1;
  xswa.save_under = True;

  XChangeWindowAttributes(config.display, config.window, CWBackingStore | CWBackingPlanes | CWSaveUnder, &xswa);

  XSelectInput(config.display, config.window, StructureNotifyMask);
  XSetStandardProperties(config.display, config.window, title, title, None, NULL, 0, &hints);

  XMapWindow(config.display, config.window);
  XSync(config.display, False);

  XSelectInput(config.display, config.window, NoEventMask);

  config.visual = DefaultVisual(config.display, DefaultScreen(config.display));
  config.depth = DefaultDepth(config.display, DefaultScreen(config.display));

  if(
    (
      config.visual->class != TrueColor &&
      config.visual->class != DirectColor
    ) ||
    config.depth < 15
  ) {
    return 3;
  }

  config.gc = XCreateGC(config.display, config.window, 0, 0);

  config.ximage = XCreateImage(
    config.display,
    config.visual,
    (unsigned) config.depth,
    ZPixmap,
    0,
    NULL,
    config.width,
    config.height,
    32,
    0
  );

  if(!config.ximage) {
    gfxboot_log("Error: failed to create XImage\n");
    XCloseDisplay(config.display);

    return 4;
  }

  config.ximage->data = calloc(1, (size_t) (config.ximage->height * config.ximage->bytes_per_line));

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int x11_close_window(void)
{
  XFreeGC(config.display, config.gc);
  XDestroyImage(config.ximage);
  XCloseDisplay(config.display);

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int x11_event_loop()
{
  int exit_event = 0;

  XSelectInput(config.display, config.window, KeyPressMask|ExposureMask|StructureNotifyMask|ButtonPressMask|PointerMotionMask);

  while(!exit_event) {
    int action __attribute__ ((unused)) = 0;
    XEvent xev;
    KeySym ks;
    // buf just needs to be large enough to hold a key code
    char buf[16], *buf_ptr;
    int buf_len, key_code;
    char *key_name;

    while(XPending(config.display)) {
      XNextEvent(config.display, &xev);
      switch(xev.type) {

        case KeyPress:
          if(config.xic) {
            buf_len = Xutf8LookupString(config.xic, (XKeyEvent *) &xev, buf, sizeof buf - 1, &ks, NULL);
          }
          else {
            buf_len = XLookupString((XKeyEvent *) &xev, buf, sizeof buf - 1, &ks, NULL);
          }
          if(buf_len >= 0) buf[buf_len + 1] = 0;
          buf_ptr = buf;
          key_code = gfx_utf8_dec(&buf_ptr, &buf_len);
          if(key_code < 0) key_code = -key_code;
          key_name = XKeysymToString(ks);
          gfxboot_debug(2, 2, "x11_event_loop: key = 0x%02x '%s'\n", key_code, key_name);
          // '^C'
          if(key_code == 0x03) exit_event = 1;
          action = x11_gfxboot_process_key(&key_code);
          break;

        case Expose:
          // gfxboot_log("XEvent.type == Expose(%d, %d, %d, %d)\n", xev.xexpose.x, xev.xexpose.y, xev.xexpose.width, xev.xexpose.height);
          break;

        case ConfigureNotify:
          // gfxboot_log("XEvent.type == ConfigureNotify\n");
          if(xev.xconfigure.width != (int) config.width || xev.xconfigure.height != (int) config.height) {
            gfxboot_log("ConfigureNotify: oops\n");
          }
          break;

        case ButtonPress:
          gfxboot_log("XEvent.type == ButtonPress\n");
          break;

        default:
          // gfxboot_log("XEvent.type == 0x%x\n", (int) xev.type);
          break;
      }
    }
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int x11_gfxboot_init()
{
  x11_gfxboot_data_free();

  gfxboot_data = calloc(1, sizeof *gfxboot_data);
  if(!gfxboot_data) return 1;

  // don't log to console
  gfxboot_data->vm.debug.log_level_console = -1;

  gfxboot_data->menu.entry = 0;
  gfxboot_data->menu.nested = 0;
  gfxboot_data->menu.timeout.max = config.timeout;

  gfxboot_data->screen.real.ptr = config.ximage->data;
  gfxboot_data->screen.real.width = (int) config.width;
  gfxboot_data->screen.real.height = (int) config.height;

  gfxboot_data->screen.real.bytes_per_line = config.ximage->bytes_per_line;
  gfxboot_data->screen.real.bytes_per_pixel = config.ximage->bitmap_pad / 8;
  gfxboot_data->screen.real.bits_per_pixel = config.ximage->bits_per_pixel;

  gfxboot_data->screen.real.red = mask_to_color_bits(config.ximage->red_mask);
  gfxboot_data->screen.real.green = mask_to_color_bits(config.ximage->green_mask);
  gfxboot_data->screen.real.blue = mask_to_color_bits(config.ximage->blue_mask);

  unsigned u = ~(config.ximage->red_mask | config.ximage->green_mask | config.ximage->blue_mask);
  u &= ((uint64_t) 1 << (gfxboot_data->screen.real.bytes_per_pixel * 8)) - 1;
  gfxboot_data->screen.real.res = mask_to_color_bits(u);

 // reserve 16 MiB for our VM
  gfxboot_data->vm.mem.size = 16 * (1 << 20);
  gfxboot_data->vm.mem.ptr = calloc(1, gfxboot_data->vm.mem.size);
  if(!gfxboot_data->vm.mem.ptr) return 1;

  if(gfxboot_init()) return 1;

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int x11_gfxboot_process_key(int *key)
{
  int action;

  if(!gfxboot_data || !key) return 0;

  gfxboot_debug(2, 2, "x11_gfxboot_process_key: key = 0x%x '%s'\n", *key, *key >= ' ' ? gfx_utf8_enc((unsigned) *key) : "");

  if(gfxboot_data->menu.timeout.current > 0) {
    // grub_env_unset("timeout");
    // grub_env_unset("fallback");
    x11_gfxboot_clear_timeout(NULL);
  }

  action = gfxboot_process_key((unsigned) *key);

  gfxboot_debug(1, 2, "x11_gfxboot_process_key: action = %d.%02x\n", action >> 8, action & 0xff);

  *key = 0;

  return action;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void x11_gfxboot_print_timeout(int timeout, void *data __attribute__ ((unused)))
{
  gfxboot_data->menu.timeout.current = timeout;

  gfxboot_timeout();
}
 

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void x11_gfxboot_clear_timeout(void *data __attribute__ ((unused)))
{
  gfxboot_data->menu.timeout.current = 0;

  gfxboot_timeout();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Print to serial line and/or console.
//
// dst - bitmask; bit 0: serial, bit 1: console
//
int gfxboot_printf(int dst, const char *format, ...)
{
  int len;
  char *s;
  int log_level_serial = (signed char) ((dst >> 8) & 0xff);
  int log_level_console = (signed char) ((dst >> 16) & 0xff);

  if(!format) return 0;

  va_list args;
  va_start(args, format);
  len = vasprintf(&s, format, args);
  va_end(args);

  if(
    (dst & 1) &&
    (!gfxboot_data || log_level_serial <= gfxboot_data->vm.debug.log_level_serial)
  ) {
    fwrite(s, (size_t) len, 1, stdout);
    fflush(stdout);
  }

  if(
    (dst & 2) &&
    gfxboot_data &&
    log_level_console <= gfxboot_data->vm.debug.log_level_console
  ) {
    char *t = s;
    while(*t) {
      if(*t == '\n') gfx_console_putc('\r', 1);
      gfx_console_putc((unsigned char) *t++, 1);
    }
  }

  free(s);

  return len;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_asprintf(char **str, const char *format, ...)
{
  int len;

  va_list args;
  va_start(args, format);
  len = vasprintf(str, format, args);
  va_end(args);

  return len;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_snprintf(char *str, unsigned size, const char *format, ...)
{
  int len;

  va_list args;
  va_start(args, format);
  len = vsnprintf(str, size, format, args);
  va_end(args);

  return len;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfxboot_sys_free(void *ptr)
{
  if(ptr) free(ptr);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned long gfxboot_sys_strlen(const char *s)
{
  return strlen(s);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_sys_strcmp(const char *s1, const char *s2)
{
  return strcmp(s1, s2);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
long int gfxboot_sys_strtol(const char *nptr, char **endptr, int base)
{
  return strtol(nptr, endptr, base);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_sys_read_file(char *name, void **buf)
{
  int size = -1;
  char *tmp = NULL;
  struct stat sbuf;
  int fd, len;

  *buf = NULL;

  if(!name) return size;

  fd = open(name, O_RDONLY);

  gfxboot_log("open(%s) = %d\n", name, fd);

  if(fd >= 0) {
    if(!fstat(fd, &sbuf)) {
      size = sbuf.st_size;
    }

    if(size > 0) {
      *buf = tmp = malloc((size_t) size);
      while(size > 0) {
        len = read(fd, tmp, (size_t) size);
        if(len <= 0) break;
        size -= len;
        tmp += len;
      }
    }

    close(fd);

    if(size > 0) {
      *buf = NULL;
      free(tmp);
      size = -1;
    }
    else {
      size = sbuf.st_size;
    }
  }

  gfxboot_log("read(%s) = %d\n", name, size);

  return size;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfxboot_screen_update(area_t area)
{
  if(opt.x11) {
    XPutImage(
      config.display, config.window, config.gc, config.ximage,
      area.x, area.y, area.x, area.y, (unsigned) area.width, (unsigned) area.height
    );
}
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_getkey()
{
  int key = 0;
  struct termios tio_old, tio;

  tcgetattr(0, &tio);
  tio_old = tio;
  tio.c_lflag &= (unsigned) ~(ICANON | ECHO);
  tcsetattr(0, TCSANOW, &tio);

  key = fgetc(stdin);

  tcsetattr(0, TCSANOW, &tio_old);

  if(key == EOF) key = 0;

  return key;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void x11_gfxboot_data_free()
{
  if(gfxboot_data) {
    free(gfxboot_data->menu.entries);
    free(gfxboot_data->vm.mem.ptr);

    free(gfxboot_data);
  }

  gfxboot_data = NULL;
}
