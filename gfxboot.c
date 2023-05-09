#include <gfxboot/gfxboot.h>
#include <gfxboot/vocabulary.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// program functions

static uint64_t gfx_decode_number(const void *data, unsigned len);
static unsigned decode_raw_instr(uint8_t *data, decoded_instr_t *instr);

static inline uint64_t tsc(void)
{
#if defined (__x86_64__)
  uint32_t eax, edx;

  asm volatile (
    "rdtsc\n"
    : "=a" (eax), "=d" (edx)
  );

  return ((uint64_t) edx << 32) + eax;
#elif defined (__i386__)
  uint64_t tsc;

  asm volatile (
    "rdtsc"
    : "=A" (tsc)
  );

  return tsc;
#else
  #error "OOPS"
  return 0;
#endif
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uint64_t gfx_decode_number(const void *data, unsigned len)
{
  const uint8_t *p = data;
  uint64_t val = 0;

  p += len;

  while(len--) {
    val <<= 8;
    val += *--p;
  }

  return val;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_program_init(obj_id_t program)
{
  gfxboot_data->vm.error = (error_t ) { };

  gfxboot_data->vm.debug.steps = 0;
  gfxboot_data->vm.debug.log_prompt = 1;

  gfxboot_data->vm.program.steps = 0;

  gfx_obj_ref_dec(gfxboot_data->vm.program.pstack);

  gfx_obj_ref_dec(gfxboot_data->vm.program.context);
  gfxboot_data->vm.program.context = 0;

  gfxboot_data->vm.program.pstack = gfx_obj_array_new(0);

  if(!gfxboot_data->vm.program.pstack) {
    GFX_ERROR(err_no_memory);
    return 0;
  }

  if(!gfx_is_code(program)) {
    GFX_ERROR(err_invalid_code);
    return 0;
  }

  obj_id_t ctx_id = gfx_obj_context_new(t_ctx_func);
  context_t *ctx = gfx_obj_context_ptr(ctx_id);
  if(!ctx) {
    GFX_ERROR(err_no_memory);
    return 0;
  }

  ctx->dict_id = gfx_obj_ref_inc(gfxboot_data->vm.program.dict);
  ctx->code_id = gfx_obj_ref_inc(program);

  // data_t *code = gfx_obj_mem_ptr(ctx->code_id);
  // gfxboot_log("program loaded %s (%u bytes)\n", gfx_obj_id2str(program), code->size);

  gfxboot_data->vm.program.context = ctx_id;

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned decode_raw_instr(uint8_t *data, decoded_instr_t *instr)
{
  unsigned inst_size = 1;

  unsigned type = data[0] & 0x0f;

  unsigned n = data[0] >> 4;

  instr->arg2 = 0;
  instr->type = type;

  if(TYPE_EXPECTS_DATA(type)) {
    unsigned len;
    if(n >= 12) {
      n -= 11;
      len = gfx_decode_number(data + 1, n);
      inst_size += n;
    }
    else {
      len = n;
    }
    instr->arg1 = len;
    instr->arg2 = data + inst_size;
    inst_size += len;
  }
  else {
    int64_t val;
    if(n >= 8) {
      n -= 7;
      val = (int64_t) gfx_decode_number(data + 1, n);
      inst_size += n;
      // ints are signed, everything else is encoded as unsigned
      if(type == t_int) {
        // expand sign bit
        val <<= 8 * (8 - n);
        val >>= 8 * (8 - n);
      }
    }
    else {
      val = n;
    }
    instr->arg1 = val;
  }

  return inst_size;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// decoded arguments are:
//
//   int64_t arg1, uint8_t[arg1] arg2
//
// arg2 is optional and may be 0; if arg2 is not 0, arg1 is its length
//
int gfx_decode_instr(decoded_instr_t *instr)
{
  *instr = (decoded_instr_t) { };

  if(gfxboot_data->vm.program.stop || gfxboot_data->vm.error.id) return 0;

  gfxboot_data->vm.program.steps++;

  obj_id_t ctx_id = gfxboot_data->vm.program.context;
  if(!ctx_id) return 0;

  instr->ctx = gfx_obj_context_ptr(ctx_id);
  if(!instr->ctx) {
    GFX_ERROR(err_internal);
    return 0;
  }

  instr->code_id = instr->ctx->code_id;

  unsigned ip = instr->ctx->ip;
  instr->ctx->current_ip = ip;

  obj_t *code_ptr = gfx_obj_ptr(instr->code_id);

  if(!code_ptr || code_ptr->base_type != OTYPE_MEM) {
    GFX_ERROR(err_invalid_code);
    return 0;
  }

  data_t *mem = OBJ_DATA_FROM_PTR(code_ptr);

  unsigned size = mem->size;
  uint8_t *data = mem->ptr;

  // at code end, do nothing
  if(ip == size) return 0;

  if(ip + 1 > size) {	// encoded instructions are at least 1 bytes
    GFX_ERROR(err_invalid_code);
    return 0;
  }

  unsigned inst_size = decode_raw_instr(data + ip, instr);

  // for a cross reference, look it up and do the same again
  if(instr->type == t_xref) {
    // xref_ip may intentionally get negative
    int xref_ip = ip - instr->arg1;

#if 0
    if(gfxboot_data->vm.debug.trace.ip) {
      gfxboot_log("IP: xref ip 0x%x\n", xref_ip);
    }
#endif

    decode_raw_instr(data + xref_ip, instr);
  }

  if(gfxboot_data->vm.debug.trace.ip) {
    gfxboot_log("IP: #%u:0x%x, type %d, ", OBJ_ID2IDX(instr->ctx->code_id), ip, instr->type);
    if(instr->arg2) {
      gfxboot_log("%d[%u]\n", (int) (instr->arg2 - data), (unsigned) instr->arg1);
    }
    else {
      gfxboot_log("%lld (0x%llx)\n", (long long) instr->arg1, (long long) instr->arg1);
    }
  }

  unsigned next_ip = ip + inst_size;

  if(next_ip > size) {
    GFX_ERROR(err_invalid_code);
    return 0;
  }

  instr->ctx->ip = next_ip;

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_program_run()
{
  uint64_t tsc_start = tsc(), tsc_last = tsc_start;

  gfxboot_data->vm.program.stop = 0;

  // gfxboot_log("running code\n");

  decoded_instr_t instr;

  unsigned steps = gfxboot_data->vm.debug.steps;
  unsigned steps_set = steps ? 1 : 0;

  for(; (!steps_set || steps) && gfx_decode_instr(&instr); steps--) {
    switch(instr.type) {
      case t_int:
      case t_bool:
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(instr.arg1, instr.type), 0);
        break;

      case t_nil:
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, 0, 0);
        break;

      case t_string:
      case t_ref:
      case t_code:
        gfx_obj_array_push(
          gfxboot_data->vm.program.pstack,
          gfx_obj_const_mem_nofree_new(instr.arg2, instr.arg1, instr.type, instr.code_id),
          0
        );
        break;

      case t_prim:
        gfx_run_prim(instr.arg1);
        break;

      case t_word:
        {
          data_t key = { .ptr = instr.arg2, .size = instr.arg1 };
          obj_id_pair_t pair = gfx_lookup_dict(&key);
          if(!pair.id1) {
            GFX_ERROR(err_invalid_code);
          }
          else {
            gfx_exec_id(0, pair.id2, 0);
          }
        }
        break;

      case t_get:
        gfx_prim_get_x(& (data_t) { .ptr = instr.arg2, .size = instr.arg1 });
        break;

      case t_set:
        {
          obj_id_t key = gfx_obj_const_mem_nofree_new(instr.arg2, instr.arg1, t_ref, instr.code_id);
          gfx_prim_put_x(key);
          gfx_obj_ref_dec(key);
        }
        break;

      case t_comment:
        // do nothing
        break;

      default:
        GFX_ERROR(err_invalid_code);
    }

    gfx_debug_show_trace();

    if(gfxboot_data->vm.debug.trace.time) {
      uint64_t tmp = tsc();
      gfxboot_log("TIME: %llu\n", (unsigned long long) tmp - tsc_last);
      tsc_last = tmp;
    }
  }

  gfxboot_data->vm.program.time += tsc() - tsc_start;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_pair_t gfx_lookup_dict(data_t *key)
{
  obj_id_pair_t pair = { };
  
  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);

  while(context) {
    if(
      context &&
      context->dict_id &&
      (pair = gfx_obj_hash_get(context->dict_id, key)).id1
    ) {
      return pair;
    }
    context = gfx_obj_context_ptr(context->parent_id);
  }

  return gfx_obj_hash_get(gfxboot_data->vm.program.dict, key);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_is_code(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_MEM || !ptr->flags.ro) {
    return 0;
  }

  data_t *mem = OBJ_DATA_FROM_PTR(ptr);

  // either we know it's a code blob or we check for the magic
  if(
    ptr->sub_type == t_code ||
    (mem->size >= 8 && gfx_decode_number(mem->ptr, 8) == (GFXBOOT_MAGIC << 8) + 0x70 + t_comment)
  ) {
    return 1;
  }

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
area_t gfx_font_dim(obj_id_t font_id)
{
  area_t area = { };
  font_t *font;

  for(; (font = gfx_obj_font_ptr(font_id)); font_id = font->parent_id) {
    if(font->width > area.width) area.width = font->width;
    if(font->height > area.height) area.height = font->height;
    if(font->line_height > area.y) area.y = font->line_height;
  }

  return area;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_image_open(obj_id_t image_file)
{
  data_t *mem = gfx_obj_mem_ptr(image_file);

  if(!mem || !mem->size) return 0;

  unsigned u = gfx_jpeg_getsize(mem->ptr);

  int width = u & 0xffff;
  int height = (int) (u >> 16);

  obj_id_t image_id = gfx_obj_canvas_new(width, height);
  canvas_t *canvas = gfx_obj_canvas_ptr(image_id);

  if(gfx_jpeg_decode(mem->ptr, (uint8_t *) &canvas->ptr, 0, canvas->geo.width, 0, canvas->geo.height, 32)) {
    gfx_obj_ref_dec(image_id);
    image_id = 0;
  } 

  return image_id;
}
