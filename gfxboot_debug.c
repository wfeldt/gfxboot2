#include <gfxboot/gfxboot.h>
#include <gfxboot/vocabulary.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static void gfx_program_debug_putc(unsigned c, unsigned cursor);
static void gfx_status_dump(void);
static void gfx_stack_dump(dump_style_t style);
static void gfx_bt_dump(dump_style_t style);

static int is_num(char *str);

static void debug_cmd_defrag(int argc, char **argv);
static void debug_cmd_dump(int argc, char **argv);
static void debug_cmd_log(int argc, char **argv);
static void debug_cmd_hex(int argc, char **argv);
static void debug_cmd_find(int argc, char **argv);
static void debug_cmd_run(int argc, char **argv);
static void debug_cmd_set(int argc, char **argv);

static char *skip_space(char *str);
static char *skip_nonspace(char *str);

static struct {
  char *name;
  void (* function)(int argc, char **argv);
} debug_cmds[] = {
  { "d", debug_cmd_dump },
  { "defrag", debug_cmd_defrag },
  { "dump", debug_cmd_dump },
  { "f", debug_cmd_run },
  { "find", debug_cmd_find },
  { "hex", debug_cmd_hex },
  { "i", debug_cmd_dump },
  { "inspect", debug_cmd_dump },
  { "log", debug_cmd_log },
  { "p", debug_cmd_dump },
  { "print", debug_cmd_dump },
  { "r", debug_cmd_run },
  { "run", debug_cmd_run },
  { "s", debug_cmd_run },
  { "set", debug_cmd_set },
  { "t", debug_cmd_run },
  { "trace", debug_cmd_run },
};


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// debug functions


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
char *gfx_debug_get_ip()
{
  static char buf[128];
  unsigned ip = 0, ref_ip = 0;
  obj_id_t code_id = 0, ref_id = 0;
  data_t *code_data = 0, *ref_data = 0;

  context_t *code_ctx = gfx_obj_context_ptr(gfxboot_data->vm.program.context);

  if(code_ctx) {
    code_id = code_ctx->code_id;
    ip = gfxboot_data->vm.error.id ? code_ctx->current_ip : code_ctx->ip;
  }

  code_data = gfx_obj_mem_ptr(code_id);
  if(code_data && code_data->ref_id) {
    ref_id = code_data->ref_id;
    ref_data = gfx_obj_mem_ptr(ref_id);
  }

  if(ref_data && code_data->ptr >= ref_data->ptr) {
    ref_ip = ip + code_data->ptr - ref_data->ptr;
  }

  gfxboot_snprintf(buf, sizeof buf, "#%u:0x%x", OBJ_ID2IDX(code_id), ip);

  if(ref_id) {
    gfxboot_snprintf(buf + gfx_strlen(buf), sizeof buf, "[#%u:0x%x]", OBJ_ID2IDX(ref_id), ref_ip);
  }

  return buf;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_debug_cmd(char *str)
{
  char *argv[16] = { };
  int i, argc = 0;
  int err = 0;
  int starts_with_space = 0;

  if(gfxboot_data->vm.debug.log_prompt) gfxboot_log("%s>%s\n", gfx_debug_get_ip(), str);

  // log comment
  if(str[0] == '#' && str[1] == ' ') {
    gfxboot_log("%s\n", str);
    return;
  }

  if(str[0] == ' ') starts_with_space = 1;

  while(argc < (int) (sizeof argv / sizeof *argv) - 1) {
    str = skip_space(str);
    if(!*str) break;
    argv[argc++] = str;
    str = skip_nonspace(str);
    if(*str) *str++ = 0;
  }

  if(!argv[0]) return;

  if(!starts_with_space) {
    for(i = 0; i < (int) (sizeof debug_cmds / sizeof *debug_cmds); i++) {
      if(!gfx_strcmp(argv[0], debug_cmds[i].name)) {
        debug_cmds[i].function(argc, argv);
        break;
      }
    }

    if(i != sizeof debug_cmds / sizeof *debug_cmds) return;
  }

  for(i = 0; i < argc; i++) {
    char *s = 0;
    char *arg = argv[i];
    unsigned arg_len = gfx_strlen(arg);
    unsigned is_id = 0;

    if(arg[0] == '#') {
      is_id = 1;
      arg++;
    }

    long val = gfx_strtol(arg, &s, 0);

    if(!gfx_strcmp(arg, "nil")) {
      gfx_obj_array_push(gfxboot_data->vm.program.pstack, 0, 0);
    }
    else if(!gfx_strcmp(arg, "true")) {
      gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(1, t_bool), 0);
    }
    else if(!gfx_strcmp(arg, "false")) {
      gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(0, t_bool), 0);
    }
    else if(s && !*s) {
      if(is_id) {
        obj_id_t id = 0;
        obj_t *ptr = gfx_obj_ptr_nocheck((unsigned) val);
        if(ptr) {
          id = OBJ_ID(OBJ_ID2IDX(val), ptr->gen);
        }
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, id, 1);
      }
      else {
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(val, t_int), 0);
      }
    }
    else if(*arg == '"') {
      if(arg_len >= 2 && arg[arg_len - 1] == '"') arg[--arg_len] = 0;
      arg++;
      arg_len--;
      obj_id_t id = gfx_obj_mem_new(arg_len, t_string);
      obj_t *ptr = gfx_obj_ptr(id);
      if(ptr) {
        gfx_memcpy(OBJ_MEM_FROM_PTR(ptr), arg, arg_len);
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, id, 0);
      }
    }
    else if(*argv[i] == '/') {
      arg++;
      arg_len--;
      obj_id_t id = gfx_obj_mem_new(arg_len, t_ref);
      obj_t *ptr = gfx_obj_ptr(id);
      if(ptr) {
        gfx_memcpy(OBJ_MEM_FROM_PTR(ptr), arg, arg_len);
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, id, 0);
      }
    }
    else if(*argv[i] == '.') {
      arg++;
      arg_len--;
      data_t key = { .ptr = arg, .size = arg_len };
      gfxboot_data->vm.program.wait_for_context = gfxboot_data->vm.program.context;
      gfx_prim_get_x(&key);
      gfx_program_run();
    }
    else if(*argv[i] == '=') {
      arg++;
      arg_len--;
      obj_id_t id = gfx_obj_mem_new(arg_len, t_ref);
      obj_t *ptr = gfx_obj_ptr(id);
      if(ptr) {
        ptr->flags.ro = 1;
        gfx_memcpy(OBJ_MEM_FROM_PTR(ptr), arg, arg_len);
        gfx_prim_put_x(id);
        gfx_obj_ref_dec(id);
      }
    }
    else {
      data_t key = { .ptr = arg, .size = arg_len };
      obj_id_t id = gfx_lookup_dict(&key).id2;
      if(id) {
        gfxboot_log("%s -> %s\n", arg, gfx_obj_id2str(id));
        gfx_exec_id(0, id, 0);
      }
      else {
        gfxboot_log("unknown command: %s\n", arg);
        err = 1;
      }
    }
  }

  if(!err) {
    gfx_debug_show_trace();
    gfx_show_error();
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_debug_show_trace()
{
  if(gfxboot_data->vm.debug.trace.context) {
    gfx_bt_dump((dump_style_t) {});
  }

  if(gfxboot_data->vm.debug.trace.pstack) {
    gfx_stack_dump((dump_style_t) {});
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_program_debug_on_off(unsigned state, unsigned input)
{
  gfxboot_data->vm.debug.console.input = input;

  if(gfxboot_data->vm.debug.console.show == state) return;

  gfxboot_data->vm.debug.console.show = state;

  if(gfxboot_data->vm.debug.console.show && state < 2) {
    // turn on minimal logging
    if(gfxboot_data->vm.debug.log_level_console < 0) {
      gfxboot_data->vm.debug.log_level_console = 0;
    }
  }

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->console.canvas_id);

  if(!canvas) return;

  gfx_screen_compose(canvas->geo);

  gfx_rect(
    gfxboot_data->console.canvas_id,
    0,
    canvas->region.height - canvas->cursor.height,
    canvas->region.width,
    canvas->cursor.height,
    canvas->bg_color
  );

  gfxboot_data->vm.debug.console.buf_pos = 0;
  gfxboot_data->vm.debug.console.buf[0] = 0;

  canvas->cursor.x = 0;
  canvas->cursor.y = canvas->region.height - canvas->cursor.height;

  if(gfxboot_data->vm.debug.console.show) {
    char buf[64];

    gfx_show_error();

    gfxboot_snprintf(buf, sizeof buf, "%s>", gfx_debug_get_ip());

    gfx_console_puts(buf);

    gfx_console_putc('_', 0);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_program_debug_putc(unsigned c, unsigned cursor)
{
  if(c) gfx_console_putc(c, 1);

  if(cursor) gfx_console_putc(cursor, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_program_debug(unsigned key)
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->console.canvas_id);

  if(!canvas) return;

  unsigned pos = gfxboot_data->vm.debug.console.buf_pos;

  if(key == 0x04) {	// ^D
    gfx_program_debug_on_off(0, 0);
    return;
  }

  if(key == 0x0d) {
    gfx_program_debug_putc(' ', 0);
    gfx_program_debug_putc(key, 0);
    gfxboot_data->vm.debug.console.buf[pos] = 0;
    gfx_debug_cmd(gfxboot_data->vm.debug.console.buf);
    gfx_program_debug_on_off(3, 1);
    return;
  }

  if(key == 0x08) {
    if(!pos) return;
    pos--;
    gfx_program_debug_putc(key, 0);
    key = 0;
  }

  if(key && key < ' ') return;

  if(
    pos >= sizeof gfxboot_data->vm.debug.console.buf - 1 ||
    canvas->cursor.x >= canvas->region.width - canvas->cursor.width
  ) {
    return;
  }

  gfx_program_debug_putc(key, '_');

  if(key) {
    // FIXME: unicode chars!
    gfxboot_data->vm.debug.console.buf[pos++] = key;
  }

  gfxboot_data->vm.debug.console.buf_pos = pos;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_program_process_key(unsigned key)
{
  int action = 0 ;

  gfxboot_debug(2, 2, "gfx_program_process_key: 0x%x\n", key);

  if(!key || !gfxboot_data->event_handler_id) return 0;

  if(gfx_program_init(gfxboot_data->event_handler_id)) {
    gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(key, t_int), 0);
    gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(1, t_int), 0);
    gfx_program_run();
    if(!gfxboot_data->vm.debug.console.show) {
      array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);
      if(pstack && pstack->size >= 1) {
        int64_t *val = gfx_obj_num_subtype_ptr(pstack->ptr[pstack->size - 1], t_int);
        if(val) action = *val;
      }
    }
  }

  return action;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_vm_status_dump()
{
  int i;

  gfxboot_log("arch bits = %d, gfxboot_data = %p\n", (int) sizeof (void *) * 8, gfxboot_data);

  gfxboot_log(
    "physical screen:\n  fb = %p, size = %d x %d (+%d)\n  pixel bytes = %d, bits = %d\n  red = %d +%d, green = %d +%d, blue = %d +%d, reserved = %d +%d\n",
    gfxboot_data->screen.real.ptr,
    gfxboot_data->screen.real.width,
    gfxboot_data->screen.real.height,
    gfxboot_data->screen.real.bytes_per_line,
    gfxboot_data->screen.real.bytes_per_pixel,
    gfxboot_data->screen.real.bits_per_pixel,
    gfxboot_data->screen.real.red.size,
    gfxboot_data->screen.real.red.pos,
    gfxboot_data->screen.real.green.size,
    gfxboot_data->screen.real.green.pos,
    gfxboot_data->screen.real.blue.size,
    gfxboot_data->screen.real.blue.pos,
    gfxboot_data->screen.real.res.size,
    gfxboot_data->screen.real.res.pos
  );

  gfxboot_log("virtual screen:\n  id = %s\n", gfx_obj_id2str(gfxboot_data->screen.canvas_id));

  canvas_t *c = gfx_obj_canvas_ptr(gfxboot_data->screen.canvas_id);

  if(c) {
    gfxboot_log(
      "  fb = %p, size = %d x %d (+%d)\n  pixel bytes = %d, bits = %d\n  red = %d +%d, green = %d +%d, blue = %d +%d, alpha = %d +%d\n",
      c->ptr, c->geo.width, c->geo.height, c->geo.width * COLOR_BYTES,
      COLOR_BYTES, COLOR_BYTES * 8,
      RED_BITS, RED_POS,
      GREEN_BITS, GREEN_POS,
      BLUE_BITS, BLUE_POS,
      ALPHA_BITS, ALPHA_POS
    );
  }

  gfxboot_log(
    "menu\n  default = %d, nested = %d\n  size = %d\n",
    gfxboot_data->menu.entry,
    gfxboot_data->menu.nested,
    gfxboot_data->menu.size
  );

  for(i = 0; i < gfxboot_data->menu.size; i++) {
    gfxboot_log("  %d: title = \"%s\"\n", i, gfxboot_data->menu.entries[i].title);
  }

  gfxboot_log(
    "vm\n  size = %d, start = 0x%lx, first free = 0x%lx\n",
    gfxboot_data->vm.mem.size,
    (unsigned long) gfxboot_data->vm.mem.first_chunk,
    (unsigned long) gfxboot_data->vm.mem.first_free
  );

  gfxboot_log(
    "debug\n  log console = %d, log serial = %d, pointer = %s, prompt = %s\n  trace =%s%s%s%s%s%s\n",
    gfxboot_data->vm.debug.log_level_console,
    gfxboot_data->vm.debug.log_level_serial,
    gfxboot_data->vm.debug.show_pointer ? "on" : "off",
    gfxboot_data->vm.debug.log_prompt ? "on" : "off",
    gfxboot_data->vm.debug.trace.ip ? " ip" : "",
    gfxboot_data->vm.debug.trace.pstack ? " stack" : "",
    gfxboot_data->vm.debug.trace.context ? " context" : "",
    gfxboot_data->vm.debug.trace.gc ? " gc" : "",
    gfxboot_data->vm.debug.trace.time ? " time" : "",
    gfxboot_data->vm.debug.trace.memcheck ? " memcheck" : ""
  );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_status_dump()
{
  canvas_t *console_canvas = gfx_obj_canvas_ptr(gfxboot_data->console.canvas_id);
  canvas_t *gfx_canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  gfxboot_log("graphics screen:\n");
  if(gfx_canvas) {
    gfxboot_log("  canvas = ");
    gfx_obj_dump(gfxboot_data->canvas_id, (dump_style_t) { .inspect = 1 });
    gfxboot_log("  cursor = %dx%d, color #%08x, bg_color #%08x\n",
      gfx_canvas->cursor.x, gfx_canvas->cursor.y,
      gfx_canvas->color, gfx_canvas->bg_color
    );
    gfxboot_log("  font = ");
    gfx_obj_dump(gfx_canvas->font_id, (dump_style_t) { .inspect = 1 });
  }

  gfxboot_log("debug console:\n");
  if(console_canvas) {
    gfxboot_log("  canvas = ");
    gfx_obj_dump(gfxboot_data->console.canvas_id, (dump_style_t) { .inspect = 1 });
    gfxboot_log("  cursor = %dx%d, color #%08x, bg_color #%08x\n",
      console_canvas->cursor.x, console_canvas->cursor.y,
      console_canvas->color, console_canvas->bg_color
    );
    gfxboot_log("  font = ");
    gfx_obj_dump(console_canvas->font_id, (dump_style_t) { .inspect = 1 });
  }

  gfxboot_log("misc:\n");
  gfxboot_log("  compose list = ");
  gfx_obj_dump(gfxboot_data->compose.list_id, (dump_style_t) { .inspect = 1 });

  gfxboot_log("  garbage collector = ");
  gfx_obj_dump(gfxboot_data->vm.gc_list, (dump_style_t) { .inspect = 1 });

  gfxboot_log("  event handler = ");
  gfx_obj_dump(gfxboot_data->event_handler_id, (dump_style_t) { .inspect = 1 });

  gfxboot_log("program:\n");
  gfxboot_log(
    "  ip = %s\n",
    gfx_debug_get_ip()
  );
  gfxboot_log("  global dict = ");
  gfx_obj_dump(gfxboot_data->vm.program.dict, (dump_style_t) { .inspect = 1 });
  gfxboot_log("  stack = ");
  gfx_obj_dump(gfxboot_data->vm.program.pstack, (dump_style_t) { .inspect = 1 });
  gfxboot_log("  context = ");
  gfx_obj_dump(gfxboot_data->vm.program.context, (dump_style_t) { .inspect = 1 });
#ifdef FULL_ERROR
  gfxboot_log(
    "  error %d (%s), src = %s:%d\n",
    gfxboot_data->vm.error.id,
    gfx_error_msg(gfxboot_data->vm.error.id),
    gfxboot_data->vm.error.src_file,
    gfxboot_data->vm.error.src_line
  );
#else
  gfxboot_log(
    "  error %d (%s)\n",
    gfxboot_data->vm.error.id,
    gfx_error_msg(gfxboot_data->vm.error.id)
  );
#endif

  uint64_t avg = gfxboot_data->vm.program.steps ? gfxboot_data->vm.program.time / gfxboot_data->vm.program.steps : 0;
  gfxboot_log(
    "  time = %llu, steps = %llu, avg = %llu/step\n",
    (unsigned long long) gfxboot_data->vm.program.time,
    (unsigned long long) gfxboot_data->vm.program.steps,
    (unsigned long long) avg
  );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_stack_dump(dump_style_t style)
{
  unsigned u;
  obj_id_t stack_id = gfxboot_data->vm.program.pstack;

  gfxboot_log("== stack (%s) ==\n", gfx_obj_id2str(stack_id));

  array_t *stack = gfx_obj_array_ptr(stack_id);

  if(!stack) return;

  for(u = 0; u < stack->size && (!style.max || u < style.max); u++) {
    gfxboot_log("  [%u] ", u);
    gfx_obj_dump(stack->ptr[stack->size - u - 1], (dump_style_t) { .inspect = 1 });
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_bt_dump(dump_style_t style)
{
  unsigned u = 0;
  obj_id_t context_id = gfxboot_data->vm.program.context;
  context_t *context;

  gfxboot_log("== backtrace ==\n");

  while((!style.max || u < style.max) && (context = gfx_obj_context_ptr(context_id))) {
    gfxboot_log("  [%u] ", u);
    gfx_obj_dump(context_id, (dump_style_t) { .inspect = 1 });
    u++;
    context_id = context->parent_id;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_dump(int argc, char **argv)
{
  dump_style_t style = { };

  if(!argv[1]) return;

  if(!gfx_strcmp(argv[0], "i")) style.inspect =1;
  if(!gfx_strcmp(argv[0], "d")) style.dump = 1;

  if(*argv[1] == '*') {
    style.ref = 1;
    argv[1]++;
  }

  if(*argv[1] == '#') argv[1]++;

  if(argv[2]) {
    style.max = (unsigned) gfx_strtol(argv[2], 0, 0);
  }

  if(!gfx_strcmp(argv[1], "st")) {
    gfx_status_dump();
  }
  else if(!gfx_strcmp(argv[1], "vm")) {
    gfx_vm_status_dump();
  }
  else if(!gfx_strcmp(argv[1], "mem")) {
    gfx_malloc_dump(style);
  }
  else if(!gfx_strcmp(argv[1], "stack")) {
    gfx_stack_dump(style);
  }
  else if(!gfx_strcmp(argv[1], "bt")) {
    gfx_bt_dump(style);
  }
  else {
    obj_id_t id = 0;
    int show_id = 1;
    char *s = 0;

    if(!gfx_strcmp(argv[1], "stack")) {
      id = gfxboot_data->vm.program.pstack;
    }
    else if(!gfx_strcmp(argv[1], "context")) {
      id = gfxboot_data->vm.program.context;
    }
    else if(!gfx_strcmp(argv[1], "dict")) {
      id = gfxboot_data->vm.program.dict;
    }
    else if(!gfx_strcmp(argv[1], "gc")) {
      id = gfxboot_data->vm.gc_list;
    }
    else if(!gfx_strcmp(argv[1], "screen")) {
      id = gfxboot_data->screen.canvas_id;
    }
    else if(!gfx_strcmp(argv[1], "canvas")) {
      id = gfxboot_data->canvas_id;
    }
    else if(!gfx_strcmp(argv[1], "consolecanvas")) {
      id = gfxboot_data->console.canvas_id;
    }
    else if(!gfx_strcmp(argv[1], "compose")) {
      id = gfxboot_data->compose.list_id;
    }
    else if(!gfx_strcmp(argv[1], "eventhandler")) {
      id = gfxboot_data->event_handler_id;
    }
    else if(!gfx_strcmp(argv[1], "ip")) {
      gfxboot_log("ip = %s\n", gfx_debug_get_ip());
      show_id = 0;
    }
    else if(!gfx_strcmp(argv[1], "err")) {
      gfx_show_error();
      show_id = 0;
    }
    else {
      unsigned idx = (unsigned) gfx_strtol(argv[1], &s, 0);
      if(*s) {
        data_t key = { .ptr = argv[1], .size = gfx_strlen(argv[1]) };
        obj_id_pair_t id_pair = gfx_lookup_dict(&key);
        if(id_pair.id1) {
          id = id_pair.id2;
        }
        else {
          show_id = 0;
        }
      }
      else {
        obj_t *ptr = gfx_obj_ptr_nocheck(idx);
        if(ptr) {
          id = OBJ_ID(OBJ_ID2IDX(idx), ptr->gen);
        }
        else {
          show_id = 0;
        }
      }
    }

    if(show_id) gfx_obj_dump(id, style);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_hex(int argc, char **argv)
{
  dump_style_t style = { .dump = 1 };
  int x = 0;
  unsigned len = 0;
  int ofs = 0;
  obj_t tmp = { };

  if(!argv[1]) return;

  if(*argv[1] == '*') {
    x = 1;
    argv[1]++;
  }

  if(*argv[1] == '#') argv[1]++;

  if(argv[2]) {
    len = (unsigned) gfx_strtol(argv[2], 0, 0);
  }

  if(argv[3]) {
    ofs = gfx_strtol(argv[3], 0, 0);
  }

  char *s = 0;
  obj_id_t idx = (obj_id_t) gfx_strtol(argv[1], &s, 0);

  if(!s || !*s) {
    obj_t *ptr = gfx_obj_ptr_nocheck(idx);
    if(ptr) {
      if(x) {
        tmp.data.ptr = ptr;
        tmp.data.size = sizeof *ptr;
      }
      else {
        if(ptr->flags.data_is_ptr) {
          tmp.data.ptr = ptr->data.ptr;
          tmp.data.size = ptr->data.size;
        }
        else {
          tmp.data.ptr = &ptr->data.value;
          tmp.data.size = 8;
        }
      }
      if(len) tmp.data.size = len;
      if(ofs) tmp.data.ptr += ofs;
      gfx_obj_mem_dump(&tmp, style);
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int is_num(char *str)
{
  return str && *str >= '0' && *str <= '9';
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_log(int argc, char **argv)
{
  argc--;
  argv++;

  while(*argv) {
    if(!gfx_strcmp(*argv, "serial")) {
      gfxboot_data->vm.debug.log_level_serial = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 0;
    }
    else if(!gfx_strcmp(*argv, "console")) {
      gfxboot_data->vm.debug.log_level_console = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 0;
    }
    else if(!gfx_strcmp(*argv, "pointer")) {
      gfxboot_data->vm.debug.show_pointer = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "prompt")) {
      gfxboot_data->vm.debug.log_prompt = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "ip")) {
      gfxboot_data->vm.debug.trace.ip = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "stack")) {
      gfxboot_data->vm.debug.trace.pstack = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "context")) {
      gfxboot_data->vm.debug.trace.context = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "gc")) {
      gfxboot_data->vm.debug.trace.gc = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "time")) {
      gfxboot_data->vm.debug.trace.time = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "memcheck")) {
      gfxboot_data->vm.debug.trace.memcheck = is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }
    else if(!gfx_strcmp(*argv, "all")) {
      gfxboot_data->vm.debug.trace.pstack =
      gfxboot_data->vm.debug.trace.context =
      gfxboot_data->vm.debug.trace.ip =
      gfxboot_data->vm.debug.trace.gc =
      gfxboot_data->vm.debug.trace.time =
      gfxboot_data->vm.debug.trace.memcheck =
      gfxboot_data->vm.debug.log_prompt =
        is_num(argv[1]) ? gfx_strtol(*++argv, 0, 0) : 1;
    }

    argv++;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_run(int argc, char **argv)
{
  unsigned steps = 0;

  if(*argv[0] == 't') steps = 1;
  if(*argv[0] == 's') gfxboot_data->vm.program.wait_for_context = gfxboot_data->vm.program.context;
  if(*argv[0] == 'f') {
    gfxboot_data->vm.program.wait_for_context = gfxboot_data->vm.program.context;
    if(gfxboot_data->vm.program.wait_for_context) {
      context_t *context_ptr = gfx_obj_context_ptr(gfxboot_data->vm.program.wait_for_context);
      if(context_ptr) {
        gfxboot_data->vm.program.wait_for_context = context_ptr->parent_id;
      }
    }
  }

  if(argv[1]) steps = (unsigned) gfx_strtol(argv[1], 0, 0);

  gfxboot_data->vm.debug.steps = steps;
  gfxboot_data->vm.error.shown = 0;

  gfx_program_run();

  gfx_show_error();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_find(int argc, char **argv)
{
  if(argc < 2) return;

  uint32_t idx = (uint32_t) gfx_strtol(argv[1], 0, 0);
  if(!idx) return;

  obj_t *ptr = gfx_obj_ptr_nocheck(idx);
  if(!ptr) return;

  obj_id_t id = OBJ_ID(idx, ptr->gen);

  ptr = gfx_obj_ptr(OBJ_ID(0, 1));
  if(!ptr) return;

  olist_t *olist = ptr->data.ptr;
  if(ptr->data.size != OBJ_OLIST_SIZE(olist->max)) {
    return;
  }

  unsigned u;

  for(u = 0; u < olist->max; u++) {
    ptr = olist->ptr + u;
    if(gfx_obj_contains_function(ptr->base_type)(ptr, id)) {
      gfxboot_log("%s\n", gfx_obj_id2str(OBJ_ID(u, ptr->gen)));
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_defrag(int argc, char **argv)
{
  if(argc < 1) return;

  uint32_t max = 0;

  if(argc == 2) max = (uint32_t) gfx_strtol(argv[1], 0, 0);

  gfx_defrag(max ?: -1u);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void debug_cmd_set(int argc, char **argv)
{
  if(argc < 3) return;

  argc--;
  argv++;

  char *s = 0;
  unsigned val = (unsigned) gfx_strtol(argv[1], &s, 0);

  if(*s) return;

  obj_id_t id = 0;

  if(val) {
    obj_t *ptr = gfx_obj_ptr_nocheck(val);
    if(ptr) {
      id = OBJ_ID(OBJ_ID2IDX(val), ptr->gen);
    }
  }

  if(!gfx_strcmp(argv[0], "stack")) {
    obj_id_t old = gfxboot_data->vm.program.pstack;
    gfxboot_data->vm.program.pstack = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "context")) {
    obj_id_t old = gfxboot_data->vm.program.context;
    gfxboot_data->vm.program.context = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "dict")) {
    obj_id_t old = gfxboot_data->vm.program.dict;
    gfxboot_data->vm.program.dict = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "gc")) {
    obj_id_t old = gfxboot_data->vm.gc_list;
    gfxboot_data->vm.gc_list = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "screen")) {
    obj_id_t old = gfxboot_data->screen.canvas_id;
    gfxboot_data->screen.canvas_id = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "canvas")) {
    obj_id_t old = gfxboot_data->canvas_id;
    gfxboot_data->canvas_id = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "consolecanvas")) {
    obj_id_t old = gfxboot_data->console.canvas_id;
    gfxboot_data->console.canvas_id = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "compose")) {
    obj_id_t old = gfxboot_data->compose.list_id;
    gfxboot_data->compose.list_id = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "eventhandler")) {
    obj_id_t old = gfxboot_data->event_handler_id;
    gfxboot_data->event_handler_id = gfx_obj_ref_inc(id);
    gfx_obj_ref_dec(old);
  }
  else if(!gfx_strcmp(argv[0], "ip")) {
    context_t *code_ctx = gfx_obj_context_ptr(gfxboot_data->vm.program.context);
    if(code_ctx) {
      code_ctx->ip = val;
    }
    gfxboot_log("ip = %s\n", gfx_debug_get_ip());
  }
  else if(!gfx_strcmp(argv[0], "err")) {
    GFX_ERROR(val);
    gfx_show_error();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
char *skip_space(char *str)
{
  while(*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;

  return str;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
char *skip_nonspace(char *str)
{
  if(str[0] == '"') {
    str++;
    while(*str != 0 && *str != '"') str++;
    if(*str == '"') str++;
  }
  else {
    while(*str && !(*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) str++;
  }

  return str;
}
