// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

// #define FULL_ERROR

typedef __UINT8_TYPE__ uint8_t;
typedef __INT8_TYPE__ int8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __INT16_TYPE__ int16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT32_TYPE__ int32_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __INT64_TYPE__ int64_t;

#if __LONG_WIDTH__ == 32
#define INCLUDE_DIV64 1
#else
#define INCLUDE_DIV64 0
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define gfx_memset __builtin_memset
#define gfx_memcpy __builtin_memmove
#define gfx_memcmp __builtin_memcmp
#define gfx_strlen gfxboot_sys_strlen
#define gfx_strcmp gfxboot_sys_strcmp
#define gfx_strtol gfxboot_sys_strtol

typedef uint32_t color_t;
typedef uint32_t obj_id_t;

#define COLOR_BYTES		((int) sizeof (color_t))
#define ALPHA_POS		24
#define ALPHA_BITS		8
#define RED_POS			16
#define RED_BITS		8
#define GREEN_POS		8
#define GREEN_BITS		8
#define BLUE_POS		0
#define BLUE_BITS		8
#define COLOR(a, r, g, b)	(((unsigned) (a) << ALPHA_POS) + ((r) << RED_POS) + ((g) << GREEN_POS) + ((b) << BLUE_POS))

#define OBJ_ID_GEN_BITS		8
#define OBJ_ID_GEN_POS		(32 - OBJ_ID_GEN_BITS)
#define OBJ_ID_GEN_MASK		~((1 << OBJ_ID_GEN_POS) - 1)

#define OBJ_TYPE_MASK		((1 << 8) - 1)

#define OBJ_ID(id, gen)		((unsigned) id + (((unsigned) gen) << OBJ_ID_GEN_POS))
#define OBJ_ID2GEN(id)		((unsigned) id >> OBJ_ID_GEN_POS)
#define OBJ_ID2IDX(id)		((unsigned) id & ~OBJ_ID_GEN_MASK)
#define OBJ_ID_ASSIGN(a, b)	do { obj_id_t tmp = a; a = gfx_obj_ref_inc(b); gfx_obj_ref_dec(tmp); } while(0)

#define OTYPE_NONE		0	// should be 0
#define OTYPE_MEM		1
#define OTYPE_OLIST		2
#define OTYPE_FONT		3
#define OTYPE_CANVAS		4
#define OTYPE_ARRAY		5
#define OTYPE_HASH		6
#define OTYPE_CONTEXT		7
#define OTYPE_NUM		8
#define OTYPE_INVALID		9
#define OTYPE_ANY		10

// internal memory size of object with size n
#define OBJ_OLIST_SIZE(n)	(sizeof (olist_t) + (n) * sizeof (obj_t))
#define OBJ_FONT_SIZE()		(sizeof (font_t))
#define OBJ_CANVAS_SIZE(w, h)	(sizeof (canvas_t) + (unsigned) (w) * (unsigned) (h) * sizeof *((canvas_t) {0}).ptr)
#define OBJ_ARRAY_SIZE(n)	(sizeof (array_t) + (n) * sizeof *((array_t) {0}).ptr)
#define OBJ_HASH_SIZE(n)	(sizeof (hash_t) + (n) * sizeof *((hash_t) {0}).ptr)
#define OBJ_CONTEXT_SIZE()	(sizeof (context_t))

#define OBJ_DATA_FROM_PTR(p)		(&(p)->data)
#define OBJ_MEM_FROM_PTR(p)		((p)->data.ptr)
#define OBJ_MEM_SIZE_FROM_PTR(p)	((p)->data.size)
#define OBJ_OLIST_FROM_PTR(p)		((olist_t *) (p)->data.ptr)
#define OBJ_FONT_FROM_PTR(p)		((font_t *) (p)->data.ptr)
#define OBJ_CANVAS_FROM_PTR(p)		((canvas_t *) (p)->data.ptr)
#define OBJ_ARRAY_FROM_PTR(p)		((array_t *) (p)->data.ptr)
#define OBJ_HASH_FROM_PTR(p)		((hash_t *) (p)->data.ptr)
#define OBJ_CONTEXT_FROM_PTR(p)		((context_t *) (p)->data.ptr)
#define OBJ_VALUE_FROM_PTR(p)		((p)->data.value)

#define ADD_AREA(a, b) (a).x += (b).x, (a).y += (b).y, (a).width += (b).width, (a).height += (b).height

#if 0
pserr_ok                        equ 0
pserr_nocode                    equ 1
pserr_invalid_opcode            equ 2
pserr_pstack_underflow          equ 3
pserr_pstack_overflow           equ 4
pserr_invalid_dict              equ 7
pserr_wrong_arg_types           equ 8
pserr_div_by_zero               equ 9
pserr_invalid_range             equ 0bh
pserr_invalid_exit              equ 0ch
pserr_invalid_image_size        equ 0dh
pserr_no_memory                 equ 0eh
pserr_invalid_data              equ 0fh
pserr_nop                       equ 10h
pserr_invalid_function          equ 11h
pserr_invalid_dict_entry        equ 200h
pserr_invalid_prim              equ 201h
#endif

#ifdef FULL_ERROR
#define GFX_ERROR(a)	gfxboot_data->vm.error = (error_t) { .id = (a), .src_file = __FILE__, .src_line = __LINE__ }
#else
#define GFX_ERROR(a)	gfxboot_data->vm.error = (error_t) { .id = (a) }
#endif

// see gfx_error_msg()
typedef enum {
  err_ok = 0, err_invalid_code, err_invalid_instruction,
  err_no_array_start, err_no_hash_start, err_no_memory,
  err_invalid_hash_key, err_stack_underflow, err_internal,
  err_no_loop_context, err_invalid_range, err_invalid_data,
  err_readonly, err_invalid_arguments, err_div_by_zero,
  err_memory_corruption
} error_id_t;

typedef struct {
  error_id_t id;
  unsigned shown:1;
#ifdef FULL_ERROR
  const char *src_file;
  int src_line;
#endif
} error_t;

typedef enum {
  mc_basic = (1 << 0),
  mc_xref = (1 << 1)
} malloc_check_t;

// dm_no_update is a bit mask
typedef enum {
  dm_merge = 0,
  dm_direct = 1,
  dm_no_update = 2
} draw_mode_t;

typedef struct {
  unsigned inspect:1;
  unsigned dump:1;
  unsigned ref:1;
  unsigned no_nl:1;
  unsigned no_head:1;
  unsigned no_check:1;
  unsigned max;
} dump_style_t;

typedef struct {
  uint32_t prev;	// offset to prev chunk (= raw size of previous chunk, first = 0)
  uint32_t next;	// offset to next chunk (= raw size of current chunk)
  obj_id_t id;		// 0 = free, otherwise obj_id using this chunk
  uint8_t data[];
} __attribute__ ((packed)) malloc_chunk_t;

typedef struct {
  void *ptr;
  void *first_chunk;
  void *first_free;
  uint32_t size;
} malloc_head_t;

typedef struct {
  const char *title;
} gfxboot_menu_t;

typedef struct {
  int pos, size;
} color_bits_t;

typedef struct {
  void *ptr;
  obj_id_t id;
  int width, height;
  int bytes_per_line;
  int bytes_per_pixel;
  int bits_per_pixel;
  color_bits_t red;
  color_bits_t green;
  color_bits_t blue;
  color_bits_t res;
} fb_t;

typedef struct {
  int x, y;
  int width, height;
} area_t;

typedef struct {
  int max_width, max_height;	// maximum canvas size; ptr[] array holds max_width * max_height pixels
  area_t geo;			// current canvas location & size; width, height <= canvas.max_width, canvas.max_height; cf. gfx_canvas_adjust_size()
  area_t region;		// FIXME: [NOT screen relative] drawing (clipping) area, relative to screen (in pixel)
  area_t cursor;		// drawing position (in x, y) and font char size (in width, height)
  color_t color;		// drawing color
  color_t bg_color;		// background color
  obj_id_t font_id;		// font
  draw_mode_t draw_mode;	// drawing mode

  color_t ptr[];
} __attribute__ ((packed)) canvas_t;

typedef union {
  struct {
    void *ptr;
    unsigned size;
    obj_id_t ref_id;		// ptr points into this object
  };
  int64_t value;
} data_t;

typedef struct {
  data_t data;			// either pointer+size or int value, see flags.data_is_ptr
  uint32_t ref_cnt;		// reference count
  uint8_t gen;			// generation
  uint8_t base_type;		// base type
  uint8_t sub_type;		// more detailed type, if != 0
  struct {
    uint8_t data_is_ptr:1;	// type of data element
    uint8_t ro:1;		// object is read-only
    uint8_t nofree:1;		// data.ptr is unmanaged and must not be freed; if data.ref_id is set, object is relative to data.ref_id
    uint8_t has_ref:1;		// object has been referenced via data.ref_id in another object
    uint8_t utf8:1;		// data is utf8 encoded
    uint8_t sticky:1;		// create new hash entries here
    uint8_t hash_is_class:1;	// hash is a class
  } flags;
} obj_t;

typedef struct {
  unsigned next;
  unsigned max;
  obj_t ptr[];
} __attribute__ ((packed)) olist_t;

typedef struct {
  unsigned type;
  obj_id_t parent_id;
  obj_id_t data_id;
  obj_id_t glyph_id;
  int width, height;
  int line_height;
  int baseline;
  unsigned glyphs;
  unsigned glyph_size;	// bitmap size per glyph in bytes
  struct {
    unsigned offset;
    unsigned size;
  } bitmap, unimap;
} font_t;

typedef struct {
  unsigned size;
  unsigned max;
  obj_id_t ptr[];
} __attribute__ ((packed)) array_t;

typedef struct key_value_s {
  obj_id_t key;
  obj_id_t value;
} key_value_t;

typedef struct {
  unsigned size;
  unsigned max;
  obj_id_t parent_id;
  key_value_t ptr[];
} __attribute__ ((packed)) hash_t;

typedef struct {
  unsigned type;
  unsigned ip, current_ip;
  obj_id_t parent_id;
  obj_id_t code_id;
  obj_id_t dict_id;
  obj_id_t iterate_id;
  int64_t index;
  int64_t max;
  int64_t inc;
} context_t;

typedef struct {
  context_t *ctx;
  int64_t arg1;
  uint8_t *arg2;
  unsigned type;
  obj_id_t code_id;
} decoded_instr_t;

typedef struct {
  obj_id_t id1, id2;
} obj_id_pair_t;

typedef struct {
  struct {
    fb_t real;			// real framebuffer
    obj_id_t canvas_id;		// canvas for internal virtual screen
  } screen;

  struct {
    obj_id_t canvas_id;		// debug console
  } console;

  obj_id_t canvas_id;		// default canvas

  struct {
    obj_id_t list_id;		// array of canvas ids
  } compose;

  obj_id_t system_id;		// system class; contains e.g. event handler

  struct {
    int nested;
    int entry;
    int size;
    gfxboot_menu_t *entries;
    struct {
      int max;
      int current;
    } timeout;
  } menu;

  struct {
    malloc_head_t mem;		// memory pool
    struct {
      olist_t *ptr;		// ptr to object list
      obj_id_t id;		// id of object list
    } olist;
    obj_id_t gc_list;		// list of objects to garbage collect
    struct {
      obj_id_t pstack;		// program data stack
      obj_id_t dict;		// global dictionary
      obj_id_t context;		// current program context
      obj_id_t wait_for_context;	// stop if this context is reached
      unsigned stop:1;
      uint64_t time;
      uint64_t steps;
    } program;
    error_t error;
    struct {
      struct {
        char buf[256];
        unsigned buf_pos;
        unsigned show:1;
        unsigned show_on_error:1;
        unsigned input:1;
      } console;
      int log_level_serial;
      int log_level_console;
      unsigned show_pointer:1;
      unsigned log_prompt:1;
      unsigned steps;
      struct {
        unsigned ip:1;
        unsigned pstack:1;
        unsigned context:1;
        unsigned gc:1;
        unsigned time:1;
        unsigned memcheck:1;
      } trace;
    } debug;
  } vm;

  struct {
    unsigned port;
  } serial;
} gfxboot_data_t;

typedef int (* dump_function_t)(obj_t *ptr, dump_style_t style);
typedef unsigned (* gc_function_t)(obj_t *ptr);
typedef int (* contains_function_t)(obj_t *ptr, obj_id_t id);
typedef unsigned (* iterate_function_t)(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern gfxboot_data_t *gfxboot_data;


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_sys_read_file(char *name, void **buf);
void gfxboot_sys_free(void *ptr);
unsigned long gfxboot_sys_strlen(const char *s);
int gfxboot_sys_strcmp(const char *s1, const char *s2);
long int gfxboot_sys_strtol(const char *nptr, char **endptr, int base);
void gfxboot_screen_update(area_t area);
int gfxboot_getkey(void);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#define gfxboot_serial(ls, ...) gfxboot_printf((((ls) & 0xff) << 8) + 1, __VA_ARGS__)
#define gfxboot_console(lc, ...) gfxboot_printf((((lc) & 0xff) << 16) + 2, __VA_ARGS__)
#define gfxboot_log(...) gfxboot_printf(3, __VA_ARGS__)
#define gfxboot_debug(ls, lc, ...) gfxboot_printf((((lc) & 0xff) << 16) + (((ls) & 0xff) << 8) + 3, __VA_ARGS__)
int gfxboot_printf(int dst, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
int gfxboot_asprintf(char **str, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
int gfxboot_snprintf(char *str, unsigned size, const char *format, ...) __attribute__ ((format (printf, 3, 4)));


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfxboot_init(int auto_run);
int gfxboot_process_key(unsigned key);
void gfxboot_timeout(void);
void gfxboot_debug_command(char *str);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint32_t gfx_read_le32(const void *p);
char *gfx_utf8_enc(unsigned uc);
int gfx_utf8_dec(char **s, unsigned *len);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_screen_compose(area_t area);
void gfx_console_putc(unsigned c, int update_pos);
void gfx_console_puts(char *s);
void gfx_putc(obj_id_t canvas_id, unsigned c, int update_pos);
void gfx_puts(obj_id_t canvas_id, char *s, unsigned len);
area_t gfx_text_dim(obj_id_t canvas_id, char *text, unsigned len);
area_t gfx_char_dim(obj_id_t canvas_id, unsigned chr);
area_t gfx_clip(area_t *area1, area_t *area2);
void gfx_blt(draw_mode_t mode, obj_id_t dst_id, area_t dst_area, obj_id_t src_id, area_t src_area);
int gfx_getpixel(obj_id_t canvas_id, int x, int y, color_t *color);
void gfx_putpixel(obj_id_t canvas_id, int x, int y, color_t color);
void gfx_line(obj_id_t canvas_id, int x0, int y0, int x1, int y1, color_t color);
void gfx_rect(obj_id_t canvas_id, int x, int y, int width, int height, color_t c);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_malloc_init(void);
void gfx_malloc_dump(dump_style_t style);
void *gfx_malloc(uint32_t size, obj_id_t id);
void gfx_free(void *ptr);
int gfx_malloc_check(malloc_check_t what);
malloc_chunk_t *gfx_malloc_find_chunk(void *ptr);
void gfx_defrag(unsigned max);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_read_file(char *name);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_init(void);
void gfx_obj_dump(obj_id_t id, dump_style_t style);
char *gfx_obj_id2str(obj_id_t id);

obj_id_t gfx_obj_new(unsigned type);
obj_id_t gfx_obj_alloc(unsigned type, uint32_t size);
obj_id_t gfx_obj_realloc(obj_id_t id, uint32_t size);

obj_id_t gfx_obj_ref_inc(obj_id_t id);
void gfx_obj_ref_dec(obj_id_t id);
unsigned gfx_obj_ref_dec_delay_gc(obj_id_t id);
void gfx_obj_run_gc(void);
unsigned gfx_obj_iterate(obj_id_t id, unsigned *idx, obj_id_t *id1, obj_id_t *id2);

obj_t *gfx_obj_ptr_nocheck(obj_id_t id);
obj_t *gfx_obj_ptr(obj_id_t id);
contains_function_t gfx_obj_contains_function(unsigned type);

obj_id_t gfx_obj_mem_new(uint32_t size, uint8_t subtype);
data_t *gfx_obj_mem_ptr(obj_id_t id);
data_t *gfx_obj_mem_ptr_rw(obj_id_t id);
data_t *gfx_obj_mem_subtype_ptr(obj_id_t id, uint8_t subtype);
int gfx_obj_mem_get(obj_id_t mem_id, int pos);
obj_id_t gfx_obj_mem_set(obj_id_t mem_id, uint8_t val, int pos);
obj_id_t gfx_obj_mem_insert(obj_id_t mem_id, uint8_t val, int pos);
int gfx_obj_mem_dump(obj_t *ptr, dump_style_t style);
unsigned gfx_obj_mem_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2);
obj_id_t gfx_obj_const_mem_nofree_new(const uint8_t *str, unsigned len, uint8_t subtype, obj_id_t ref_id);
obj_id_t gfx_obj_asciiz_new(const char *str);
obj_id_t gfx_obj_mem_dup(obj_id_t id, unsigned extra_bytes);
int gfx_obj_mem_cmp(data_t *mem1, data_t *mem2);
void gfx_obj_mem_del(obj_id_t mem_id, int pos);
unsigned gfx_obj_mem_gc(obj_t *ptr);
int gfx_obj_mem_contains(obj_t *ptr, obj_id_t id);

obj_id_t gfx_obj_olist_new(unsigned max);
olist_t *gfx_obj_olist_ptr(obj_id_t id);
int gfx_obj_olist_dump(obj_t *ptr, dump_style_t style);

obj_id_t gfx_obj_font_new(void);
font_t *gfx_obj_font_ptr(obj_id_t id);
int gfx_obj_font_dump(obj_t *ptr, dump_style_t style);
unsigned gfx_obj_font_gc(obj_t *ptr);
int gfx_obj_font_contains(obj_t *ptr, obj_id_t id);
obj_id_t gfx_obj_font_open(obj_id_t font_file);

obj_id_t gfx_obj_canvas_new(int width, int height);
canvas_t *gfx_obj_canvas_ptr(obj_id_t id);
int gfx_obj_canvas_dump(obj_t *ptr, dump_style_t style);
int gfx_canvas_adjust_size(canvas_t *c, int width, int height);
int gfx_canvas_resize(obj_id_t canvas_id, int width, int height);
unsigned gfx_obj_canvas_gc(obj_t *ptr);
int gfx_obj_canvas_contains(obj_t *ptr, obj_id_t id);

obj_id_t gfx_obj_array_new(unsigned max);
array_t *gfx_obj_array_ptr(obj_id_t id);
array_t *gfx_obj_array_ptr_rw(obj_id_t id);
unsigned gfx_obj_array_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2);
int gfx_obj_array_dump(obj_t *ptr, dump_style_t style);
obj_id_t gfx_obj_array_set(obj_id_t array_id, obj_id_t id, int pos, int do_ref_cnt);
obj_id_t gfx_obj_array_insert(obj_id_t array_id, obj_id_t id, int pos, int do_ref_cnt);
obj_id_t gfx_obj_array_get(obj_id_t array_id, int pos);
obj_id_t gfx_obj_array_push(obj_id_t array_id, obj_id_t id, int do_ref_cnt);
obj_id_t gfx_obj_array_pop(obj_id_t array_id, int do_ref_cnt);
void gfx_obj_array_pop_n(unsigned n, obj_id_t array_id, int do_ref_cnt);
obj_id_t gfx_obj_array_add(obj_id_t array_id, obj_id_t id, int do_ref_cnt);
unsigned gfx_obj_array_gc(obj_t *ptr);
int gfx_obj_array_contains(obj_t *ptr, obj_id_t id);
void gfx_obj_array_del(obj_id_t array_id, int pos, int do_ref_cnt);

obj_id_t gfx_obj_hash_new(unsigned max);
hash_t *gfx_obj_hash_ptr(obj_id_t id);
hash_t *gfx_obj_hash_ptr_rw(obj_id_t id);
unsigned gfx_obj_hash_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2);
int gfx_obj_hash_dump(obj_t *ptr, dump_style_t style);
obj_id_t gfx_obj_hash_set(obj_id_t hash_id, obj_id_t key_id, obj_id_t value_id, int do_ref_cnt);
obj_id_pair_t gfx_obj_hash_get(obj_id_t hash_id, data_t *key);
void gfx_obj_hash_del(obj_id_t hash_id, obj_id_t key_id, int do_ref_cnt);
unsigned gfx_obj_hash_gc(obj_t *ptr);
int gfx_obj_hash_contains(obj_t *ptr, obj_id_t id);

obj_id_t gfx_obj_context_new(uint8_t sub_type);
context_t *gfx_obj_context_ptr(obj_id_t id);
int gfx_obj_context_dump(obj_t *ptr, dump_style_t style);
unsigned gfx_obj_context_gc(obj_t *ptr);
int gfx_obj_context_contains(obj_t *ptr, obj_id_t id);

obj_id_t gfx_obj_num_new(int64_t num, uint8_t subtype);
int64_t *gfx_obj_num_ptr(obj_id_t id);
int64_t *gfx_obj_num_subtype_ptr(obj_id_t id, uint8_t subtype);
int gfx_obj_num_dump(obj_t *ptr, dump_style_t style);

unsigned gfx_program_init(obj_id_t program);
int gfx_decode_instr(decoded_instr_t *code);
void gfx_program_run(void);
void gfx_program_debug(unsigned key);
void gfx_program_debug_on_off(unsigned state, unsigned input);
int gfx_program_process_key(unsigned key);
char *gfx_debug_get_ip(void);
void gfx_debug_cmd(char *str);
void gfx_debug_show_trace(void);
void gfx_vm_status_dump(void);
obj_id_pair_t gfx_lookup_dict(data_t *key);
int gfx_is_code(obj_id_t id);
area_t gfx_font_dim(obj_id_t font_id);
obj_id_t gfx_image_open(obj_id_t image_file);

const char *gfx_error_msg(error_id_t id);
void gfx_show_error(void);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// jpeg
int gfx_jpeg_decode(uint8_t *jpeg, uint8_t *img, int x_0, int x_1, int y_0, int y_1, int color_bits);
unsigned gfx_jpeg_getsize(uint8_t *buf);

unsigned gfx_png_getsize(uint8_t *buf, unsigned len);
unsigned gfx_png_decode(uint8_t *in_buf, unsigned in_len, uint8_t *out_buf, unsigned out_len, uint8_t *window_buf);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// gfxboot_prim.c
//
int gfx_setup_dict(void);
error_id_t gfx_run_prim(unsigned prim);
void gfx_exec_id(obj_id_t dict, obj_id_t id, int on_stack);
void gfx_prim_get_x(data_t *key);
void gfx_prim_put_x(obj_id_t id);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// general id/ptr pair handling
//
typedef struct {
  obj_id_t id;
  obj_t *ptr;
} arg_t;

static void __attribute__((unused)) arg_update(arg_t *arg) { arg->ptr = gfx_obj_ptr(arg->id); }
