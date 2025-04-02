#include <gfxboot/gfxboot.h>

#define WITH_PRIM_NAMES 1
#define WITH_PRIM_HEADERS 1
#include <gfxboot/vocabulary.h>

static obj_id_t prim_array_start_id = 0;
static obj_id_t prim_hash_start_id = 0;

typedef enum {
  op_sub, op_mul, op_div, op_mod, op_and, op_or, op_xor, op_min, op_max, op_shl, op_shr,
  op_neg, op_not, op_abs
} op_t;

typedef enum {
  op_eq, op_ne, op_gt, op_ge, op_lt, op_le, op_cmp
} cmp_op_t;

typedef struct {
  obj_id_t id;
  obj_t *ptr;
} arg_t;

static arg_t *gfx_arg_1(uint8_t type);
static arg_t *gfx_arg_n(unsigned argc, uint8_t arg_types[]);
static int is_true(obj_id_t id);
static error_id_t do_op(op_t op, int64_t *result, int64_t val1, int64_t val2, unsigned sub_type);
static void binary_op_on_stack(op_t op);
static void unary_op_on_stack(op_t op);
static void binary_cmp_on_stack(cmp_op_t op);
static void gfx_prim_def_at(unsigned where);
static void gfx_prim__add(unsigned direct);

#define IS_NIL		0x80
#define IS_RW		0x40
#define TYPE_MASK	0x3f

#define OBJ_PTR_UPDATE(a) (a).ptr = gfx_obj_ptr((a).id)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
arg_t *gfx_arg_1(uint8_t type)
{
  static arg_t argv[1];
  unsigned is_nil = type & IS_NIL;
  unsigned is_rw = type & IS_RW;
  type &= TYPE_MASK;

  GFX_ERROR(err_ok);

  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);

    return 0;
  }

  obj_id_t id = argv[0].id = pstack->ptr[pstack->size - 1];
  obj_t *ptr = argv[0].ptr = gfx_obj_ptr(id);

  if(
    (ptr && (type == ptr->base_type || type == OTYPE_ANY)) ||
    (id == 0 && (type == OTYPE_NONE || is_nil))
  ) {
    if(is_rw && ptr && ptr->flags.ro) {
      GFX_ERROR(err_readonly);

      return 0;
    }

    return argv;
  }

  GFX_ERROR(err_invalid_arguments);

  return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
arg_t *gfx_arg_n(unsigned argc, uint8_t arg_types[])
{
  unsigned i;
  static arg_t argv[8];

#if 0
  if(argc >= sizeof argv / sizeof *argv) {
    GFX_ERROR(err_internal);

    return 0;
  }
#endif

  GFX_ERROR(err_ok);

  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < argc) {
    GFX_ERROR(err_stack_underflow);

    return 0;
  }

  unsigned stack_size = pstack->size;
  obj_id_t *stack = pstack->ptr + stack_size - argc;

  for(i = 0; i < argc; i++) {
    obj_id_t id = argv[i].id = stack[i];
    obj_t *ptr = argv[i].ptr = gfx_obj_ptr(id);

    uint8_t type = arg_types[i];
    unsigned is_nil = type & IS_NIL;
    unsigned is_rw = type & IS_RW;
    type &= TYPE_MASK;

    if(
      (ptr && (type == ptr->base_type || type == OTYPE_ANY)) ||
      (id == 0 && (type == OTYPE_NONE || is_nil))
    ) {
      if(is_rw && ptr && ptr->flags.ro) {
        GFX_ERROR(err_readonly);

        return 0;
      }
      continue;
    }

    GFX_ERROR(err_invalid_arguments);

    return 0;
  }

  return argv;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// return: 0 = failed, 1 = ok
//
int gfx_setup_dict()
{
  unsigned u;
  obj_id_t prim_id;

  gfx_obj_ref_dec(gfxboot_data->vm.program.dict);
  gfxboot_data->vm.program.dict = gfx_obj_hash_new(0);
  if(!gfxboot_data->vm.program.dict) return 0;

  for(u = 0; u < sizeof prim_names / sizeof *prim_names ; u++) {
    gfx_obj_hash_set(
      gfxboot_data->vm.program.dict,
      gfx_obj_const_mem_nofree_new((const uint8_t *) prim_names[u], gfx_strlen(prim_names[u]), t_ref, 0),
      prim_id = gfx_obj_num_new(u, t_prim),
      0
    );

    switch(u) {
      case prim_idx_array_start:
        prim_array_start_id = prim_id;
        break;
      case prim_idx_hash_start:
        prim_hash_start_id = prim_id;
        break;
    }
  }

  if(!prim_array_start_id || !prim_hash_start_id) return 0;

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
error_id_t gfx_run_prim(unsigned prim)
{
  if(prim > sizeof gfx_prim_list / sizeof *gfx_prim_list) {
    GFX_ERROR(err_invalid_code);
    return gfxboot_data->vm.error.id;
  }

  gfx_prim_list[prim]();

  return gfxboot_data->vm.error.id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// start code block
//
// group: code,array,hash
//
// ( -- code_1 )
//
// Put a reference to the following code block on the stack. The code block
// starts after the opening `{` and extends to (and including) the matching
// closing `}`.
//
// This special word is handled while converting the source code into binary code with `gfxboot-compile`.
// For this reason, this is the only word that cannot be redefined.
//
// example:
//
// { "Hello!" show }
//
void gfx_prim_code_start()
{
  // will never be called
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// finish code block
//
// group: code,array,hash
//
// ( -- )
//
// This marks the end of a code block. When the code is executed, the
// interpreter leaves the current execution context and returns to the
// parent context.
//
// example:
//
// /hello { "Hello!" show } def
// hello                                # print "Hello!"
//
void gfx_prim_code_end()
{
  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);

  if(!context) {
    GFX_ERROR(err_invalid_instruction);
    return;
  }

  obj_id_t parent_id = context->parent_id;

  switch(context->type) {
    case t_ctx_block:
    case t_ctx_func:
      OBJ_ID_ASSIGN(gfxboot_data->vm.program.context, parent_id);
      break;

    case t_ctx_loop:
      context->ip = 0;
      break;

    case t_ctx_repeat:
      if(--context->index) {
        context->ip = 0;
      }
      else {
        OBJ_ID_ASSIGN(gfxboot_data->vm.program.context, parent_id);
      }
      break;

    case t_ctx_for:
      context->index += context->inc;
      if(
        (context->inc > 0 && context->index <= context->max) ||
        (context->inc < 0 && context->index >= context->max)
      ) {
        context->ip = 0;
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(context->index, t_int), 0);
      }
      else {
        OBJ_ID_ASSIGN(gfxboot_data->vm.program.context, parent_id);
      }
      break;

    case t_ctx_forall:
      ;
      obj_id_t val1, val2;
      unsigned idx = context->index;
      unsigned items = gfx_obj_iterate(context->iterate_id, &idx, &val1, &val2);
      context->index = idx;

      if(items) {
        context->ip = 0;
        // note: reference counting for val1 and val2 has been done inside gfx_obj_iterate()
        gfx_obj_array_push(gfxboot_data->vm.program.pstack, val1, 0);
        if(items > 1) gfx_obj_array_push(gfxboot_data->vm.program.pstack, val2, 0);
      }
      else {
        OBJ_ID_ASSIGN(gfxboot_data->vm.program.context, parent_id);
      }
      break;

    default:
      GFX_ERROR(err_invalid_instruction);
      break;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// start array definition
//
// group: array,code,hash
//
// ( -- mark_1 )
//
// mark_1:		array start marker
//
// Put array start marker on stack. Array definition is completed with ].
//
// example:
//
// [ 1 2 3 ]            # array with 3 elements
//
void gfx_prim_array_start()
{
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, prim_array_start_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// finish array definition
//
// group: array,code,hash
//
// ( mark_1 any_1 ... any_n -- array_1 )
//
// mark_1:		array start marker
// any_1 ... any_n:	some elements
// array_1:		new array
//
// Search for mark_1 on the stack and put everything between mark_1 and TOS
// into an array.
//
// example:
//
// [ 10 20 "some" "text" ]      # array with 4 elements
//
void gfx_prim_array_end()
{
  array_t *a = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!a) return;

  unsigned idx = a->size;
  unsigned start_idx = -1u;

  while(idx-- > 0) {
    if(a->ptr[idx] == prim_array_start_id) {
      start_idx = idx;
      break;
    }
  }

  if(start_idx == -1u) {
    GFX_ERROR(err_no_array_start);
    return;
  }

  obj_id_t array_id = gfx_obj_array_new(a->size - start_idx - 1);

  if(!array_id) {
    GFX_ERROR(err_no_memory);
    return;
  }

  // no ref counting neded as the elements are just moved
  for(idx = start_idx + 1; idx < a->size; idx++) {
    gfx_obj_array_push(array_id, a->ptr[idx], 0);
  }

  a->size = start_idx + 1;
  a->ptr[start_idx] = array_id;

  gfx_obj_ref_dec(prim_array_start_id);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// start hash definition
//
// group: hash,array,code
//
// ( -- mark_1 )
//
// mark_1:		hash start marker
//
// Put hash start marker on stack. Hash definition is completed with ).
//
// example:
//
// ( "foo" 10 "bar" 20 )        # hash with 2 keys "foo" and "bar"
//
void gfx_prim_hash_start()
{
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, prim_hash_start_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// finish hash definition
//
// group: hash,array,code
//
// ( mark_1 any_1 ... any_n -- hash_1 )
//
// mark_1:		array start marker
// any_1 ... any_n:	some key - value pairs
// hash_1:		new hash
//
// Search for mark_1 on the stack and put everything between mark_1 and TOS
// into a hash. The elements are interpreted alternatingly as key and value.
// If there's an odd number of elements on the stack, the last value is nil.
//
// example:
//
// ( "foo" 10 "bar" 20 )        # hash with 2 keys "foo" and "bar"
//
void gfx_prim_hash_end()
{
  array_t *a = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!a) return;

  unsigned idx = a->size;
  unsigned start_idx = -1u;

  while(idx-- > 0) {
    if(a->ptr[idx] == prim_hash_start_id) {
      start_idx = idx;
      break;
    }
  }

  if(start_idx == -1u) {
    GFX_ERROR(err_no_hash_start);
    return;
  }

  obj_id_t hash_id = gfx_obj_hash_new((a->size - start_idx) / 2);

  if(!hash_id) {
    GFX_ERROR(err_no_memory);
    return;
  }

  // no ref counting neded as the elements are just moved
  for(idx = start_idx + 1; idx < a->size; idx += 2) {
    obj_id_t key = a->ptr[idx];
    obj_id_t val = idx + 1 < a->size ? a->ptr[idx + 1] : 0;
    if(gfx_obj_mem_ptr(key)) {
      gfx_obj_hash_set(hash_id, key, val, 0);
    }
    else {
      GFX_ERROR(err_invalid_hash_key);
      gfx_obj_ref_dec(hash_id);
      return;
    }
  }

  a->size = start_idx + 1;
  a->ptr[start_idx] = hash_id;

  gfx_obj_ref_dec(prim_hash_start_id);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// where: 0 (existing), 1 (local), 2 (global)
//
void gfx_prim_def_at(unsigned where)
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);

  if(!ptr1 || ptr1->sub_type != t_ref) {
    GFX_ERROR(err_invalid_instruction);
    return;
  }

  obj_id_t dict_id = 0;

  if(where == 0) {
    obj_id_pair_t pair = gfx_lookup_dict(OBJ_DATA_FROM_PTR(ptr1));
    if(pair.id1) {
      dict_id = pair.id1;
    }
    else {
      where = 1;
    }
  }

  if(where) {
    obj_id_t context_id = gfxboot_data->vm.program.context;
    context_t *context = gfx_obj_context_ptr(context_id);
    if(!context) {
      GFX_ERROR(err_internal);
      return;
    }

    if(where == 2) {
      while(context->parent_id) {
        context = gfx_obj_context_ptr(context_id = context->parent_id);
        if(!context) {
          GFX_ERROR(err_internal);
          return;
        }
      }
    }

    if(!context->dict_id) {
      // careful: gfx_obj_hash_new() may invalidate context pointer
      obj_id_t tmp_id = gfx_obj_hash_new(0);
      context = gfx_obj_context_ptr(context_id);
      if(!context) {
        GFX_ERROR(err_internal);
        return;
      }
      context->dict_id = tmp_id;
    }

    dict_id = context->dict_id;

    if(!dict_id) {
      GFX_ERROR(err_internal);
      return;
    }
  }

  gfx_obj_hash_set(dict_id, id1, id2, 1);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// define new word
//
// group: def
//
// ( word_1 any_1 -- )
//
// If word_1 does not exist, define word_1 in the current context.
//
// If word_1 does already exist, redefine word_1 in the context in which it is defined.
//
// example:
// /x 100 def           # define x as 100
// /neg { -1 mul } def  # define a function that negates its argument
//
void gfx_prim_def()
{
  gfx_prim_def_at(0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// define new local word
//
// group: def
//
// ( word_1 any_1 -- )
//
// Define word_1 in the current local context.
//
// example:
// /foo 200 ldef        # define local word foo as 200
//
void gfx_prim_ldef()
{
  gfx_prim_def_at(1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// define new global word
//
// group: def
//
// ( word_1 any_1 -- )
//
// Define word_1 in the global context.
//
// example:
// /foo 300 gdef        # define global word foo as 300
//
void gfx_prim_gdef()
{
  gfx_prim_def_at(2);
}


#if 0
/* not really needed... */
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_prim_class()
{
  arg_t *argv;

  argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_MEM, OTYPE_HASH, OTYPE_HASH | IS_RW });

  if(argv) {
    if(argv[0].ptr->sub_type != t_ref) {
      GFX_ERROR(err_invalid_arguments);
      return;
    }

    context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);
    if(!context) {
      GFX_ERROR(err_internal);
      return;
    }

    while(context->parent_id) {
      context = gfx_obj_context_ptr(context->parent_id);
      if(!context) {
        GFX_ERROR(err_internal);
        return;
      }
    }

    if(!context->dict_id) context->dict_id = gfx_obj_hash_new(0);

    obj_id_t dict_id = context->dict_id;

    gfx_obj_hash_set(dict_id, argv[0].id, argv[2].id, 1);

    hash_t *hash = OBJ_HASH_FROM_PTR(argv[2].ptr);
    obj_id_t old_id = hash->parent_id;
    hash->parent_id = gfx_obj_ref_inc(argv[1].id);
    gfx_obj_ref_dec(old_id);

    gfx_obj_array_pop_n(3, gfxboot_data->vm.program.pstack, 1);
  }
}
#endif


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// conditional execution
//
// group: if
//
// (bool_1 code_1 -- )
// (int_1 code_1 -- )
// (nil code_1 -- )
// (any_1 code_1 -- )
//
// code_1: code block to run if condition evaluates to 'true'
//
// The condition is false for: boolean false, integer 0, or nil. In all other cases it is true.
//
// example:
//
// true { "ok" show } if        # "ok"
// 50 { "ok" show } if          # "ok"
// nil { "ok" show } if         # shows nothing
// "" { "ok" show } if          # "ok"
//
void gfx_prim_if()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  if(!gfx_is_code(id2)) {
    GFX_ERROR(err_invalid_code);
    return;
  }

  if(is_true(id1)) {
    obj_id_t context_id = gfx_obj_context_new(t_ctx_block);
    context_t *context = gfx_obj_context_ptr(context_id);

    if(!context) {
      GFX_ERROR(err_no_memory);
      return;
    }

    context->code_id = gfx_obj_ref_inc(id2);

    context->parent_id = gfxboot_data->vm.program.context;
    gfxboot_data->vm.program.context = context_id;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// conditional execution
//
// group: if
//
// (bool_1 code_1 code_2 -- )
// (int_1 code_1 code_2 -- )
// (nil code_1 code_2 -- )
// (any_1 code_1 code_2 -- )
//
// code_1: code block to run if condition evaluates to 'true'
// code_2: code block to run if condition evaluates to 'false'
//
// The condition is false for: boolean false, integer 0, or nil. In all other cases it is true.
//
// example:
//
// false { "ok" } { "bad" } ifelse show         # "bad"
// 20 { "ok" } { "bad" } ifelse show            # "ok"
// nil { "ok" } { "bad" } ifelse sho            # "bad"
// "" { "ok" } { "bad" } ifelse show            # "ok"
//
void gfx_prim_ifelse()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 3) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 3];
  obj_id_t id2 = pstack->ptr[pstack->size - 2];
  obj_id_t id3 = pstack->ptr[pstack->size - 1];

  if(!gfx_is_code(id2) || !gfx_is_code(id3)) {
    GFX_ERROR(err_invalid_code);
    return;
  }

  obj_id_t context_id = gfx_obj_context_new(t_ctx_block);
  context_t *context = gfx_obj_context_ptr(context_id);

  if(!context) {
    GFX_ERROR(err_no_memory);
    return;
  }

  context->code_id = gfx_obj_ref_inc(is_true(id1) ? id2 : id3);

  context->parent_id = gfxboot_data->vm.program.context;
  gfxboot_data->vm.program.context = context_id;

  for(int i = 0; i < 3; i++) gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// endless loop
//
// group: if,loop
//
// ( code_1 -- )
//
// Repeat code_1 forever until you exit the loop explicitly.
//
// example:
//
// { "Help!" show } loop
//
void gfx_prim_loop()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];

  if(!gfx_is_code(id1)) {
    GFX_ERROR(err_invalid_code);
    return;
  }

  obj_id_t context_id = gfx_obj_context_new(t_ctx_loop);
  context_t *context = gfx_obj_context_ptr(context_id);

  if(!context) {
    GFX_ERROR(err_no_memory);
    return;
  }

  context->code_id = gfx_obj_ref_inc(id1);

  context->parent_id = gfxboot_data->vm.program.context;
  gfxboot_data->vm.program.context = context_id;

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// repeat code block
//
// group: if,loop
//
// ( int_1 code_1 -- )
//
// Repeat code_1 int_1 times. If int_1 is less or equal to 0, code_1 is not run.
//
// example:
//
// 3 { "Help!" show } repeat            # "Help!Help!Help!"
//
void gfx_prim_repeat()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  int64_t *count = gfx_obj_num_subtype_ptr(id1, t_int);

  if(!count || !gfx_is_code(id2)) {
    GFX_ERROR(err_invalid_code);
    return;
  }

  if(*count > 0) {
    obj_id_t context_id = gfx_obj_context_new(t_ctx_repeat);
    context_t *context = gfx_obj_context_ptr(context_id);

    if(!context) {
      GFX_ERROR(err_no_memory);
      return;
    }

    context->code_id = gfx_obj_ref_inc(id2);
    context->index = *count;

    context->parent_id = gfxboot_data->vm.program.context;
    gfxboot_data->vm.program.context = context_id;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// run code block repeatedly, with counter
//
// group: if,loop
//
// ( int_1 int_2 int_3 code_1 -- )
// int_1: start value
// int_2: increment value
// int_3: maximum value (inclusive)
//
// Run code_1 repeatedly and put the current counter value on the stack in every iteration.
//
// The counter starts with int_1 and is incremented by int_2 until it
// reaches int_3. The code block is executed with the start value and then
// as long as the counter is less than or equal to the maximum value.
//
// The increment may be negative. In that case the loop is executed as long as the counter
// is greater than or equal to the maximum value.
//
// If the increment is 0, the loop is not executed.
//
// example:
//
// 0 1 4 { } for                # 0 1 2 3 4
// 0 -2 -5 { } for              # 0 -2 -4
//
void gfx_prim_for()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 4) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 4];
  obj_id_t id2 = pstack->ptr[pstack->size - 3];
  obj_id_t id3 = pstack->ptr[pstack->size - 2];
  obj_id_t id4 = pstack->ptr[pstack->size - 1];

  int64_t *start = gfx_obj_num_subtype_ptr(id1, t_int);
  int64_t *inc = gfx_obj_num_subtype_ptr(id2, t_int);
  int64_t *max = gfx_obj_num_subtype_ptr(id3, t_int);

  if(!start || !inc || !max || !gfx_is_code(id4)) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  int pop_count = 4;

  if(
    (*inc > 0 && *start <= *max) ||
    (*inc < 0 && *start >= *max)
  ) {
    obj_id_t context_id = gfx_obj_context_new(t_ctx_for);
    context_t *context = gfx_obj_context_ptr(context_id);

    if(!context) {
      GFX_ERROR(err_no_memory);
      return;
    }

    context->code_id = gfx_obj_ref_inc(id4);
    context->index = *start;
    context->inc = *inc;
    context->max = *max;

    context->parent_id = gfxboot_data->vm.program.context;
    gfxboot_data->vm.program.context = context_id;

    pop_count--;
  }

  for(int i = 0; i < pop_count; i++) gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// loop over all elements
//
// group: if,loop
//
// ( array_1 code_1 -- )
// ( hash_1 code_1 -- )
// ( string_1 code_1 -- )
//
// Run code_1 for each element of array_1, hash_1, or string_1.
//
// For array_1 and string_1, each element is put on the stack and code_1 is run.
//
// For hash_1, each key and value pair are put on the stack and code_1 is run.
// The hash keys are iterated in alphanumerical order.
//
// Note that string_1 is interpreted as a sequence of bytes, not UTF8-encoded characters.
//
// example:
//
// [ 10 20 30 ] { } forall              # 10 20 30
// ( "foo" 10 "bar" 20 ) { } forall     # "bar" 20 "foo" 10
// "ABC" { } forall                     # 65 66 67
//
void gfx_prim_forall()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);

  if(
    !gfx_is_code(id2) ||
    !ptr1 ||
    (ptr1->base_type != OTYPE_ARRAY && ptr1->base_type != OTYPE_HASH && ptr1->base_type != OTYPE_MEM)
  ) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  obj_id_t val1, val2;
  unsigned idx = 0;
  unsigned items = gfx_obj_iterate(id1, &idx, &val1, &val2);

  if(items) {
    obj_id_t context_id = gfx_obj_context_new(t_ctx_forall);
    context_t *context = gfx_obj_context_ptr(context_id);

    if(!context) {
      GFX_ERROR(err_no_memory);
      return;
    }

    context->code_id = gfx_obj_ref_inc(id2);
    context->index = idx;
    context->iterate_id = gfx_obj_ref_inc(id1);

    context->parent_id = gfxboot_data->vm.program.context;
    gfxboot_data->vm.program.context = context_id;

    gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
    gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

    // note: reference counting for val1 and val2 has been done inside gfx_obj_iterate()
    gfx_obj_array_push(gfxboot_data->vm.program.pstack, val1, 0);
    if(items > 1) gfx_obj_array_push(gfxboot_data->vm.program.pstack, val2, 0);
  }
  else {
    gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
    gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// leave loop/repeat/for/forall loop
//
// group: if,loop
//
// ( -- )
//
// Exit from current loop.
//
// example:
//
// 0 1 10 { dup 4 eq { exit } if } for          # 0 1 2 3 4
//
void gfx_prim_exit()
{
  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);

  if(!context) {
    GFX_ERROR(err_internal);
    return;
  }

  for(; context; context = gfx_obj_context_ptr(context->parent_id)) {
    if(context->type == t_ctx_block) continue;

    if(context->type == t_ctx_func) {
      GFX_ERROR(err_no_loop_context);
      return;
    }

    OBJ_ID_ASSIGN(gfxboot_data->vm.program.context, context->parent_id);

    return;
  }

  GFX_ERROR(err_no_loop_context);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// leave current function
//
// group: if,loop
//
// Exit from currently running function.
//
// example:
//
// /foo { dup nil eq { return } if show } def
// "abc" foo                                    # shows "abc"
// nil foo                                      # does nothing
//
void gfx_prim_return()
{
  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);

  if(!context) {
    GFX_ERROR(err_internal);
    return;
  }

  for(; context; context = gfx_obj_context_ptr(context->parent_id)) {
    if(context->type == t_ctx_func) break;
  }

  if(!context) gfxboot_data->vm.program.stop = 1;

  OBJ_ID_ASSIGN(gfxboot_data->vm.program.context, context ? context->parent_id : 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// create or duplicate string
//
// group: mem
//
// ( int_1 -- string_1 )
// int_1: length
// string_1: new string with length int_1
// ( string_2 -- string_3 )
// string_2: string to duplicate
// string_3: copy of string_2
//
// There are two variants: given a number, a string of that length is
// created and initialized with zeros; given a string, a copy of that string is created.
//
// int_1 may be 0 to create a zero-length string.
//
// Note: duplication works for all string-like objects. For example for word references and even code blocks.
//
// example:
//
// 2 string                    # creates an empty string of length 2: "\x00\x00"
// "abc" string                # creates a copy of "abc"
//
// # even this works:
// /abc string                 # a copy of /abc
// { 10 20 } string            # a copy of the code block { 10 20 }
//
void gfx_prim_string()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  if(!ptr1) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  obj_id_t val_id = 0;

  switch(ptr1->base_type) {
    case OTYPE_NUM:
      {
        int64_t value = OBJ_VALUE_FROM_PTR(ptr1);

        if(value < 0 || value >= (1ll << 32)) {
          GFX_ERROR(err_invalid_range);
          return;
        }

        val_id = gfx_obj_mem_new(value, 0);

        if(!val_id) {
          GFX_ERROR(err_no_memory);
          return;
        }
      }
      break;

    case OTYPE_MEM:
      // FIXME: change to set sub_type to t_string?
      val_id = gfx_obj_mem_dup(id1, 0);
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  // we did the ref counting already above
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, val_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get array, hash, or string element
//
// group: get
//
// ( array_1 int_1 -- )
// array_1: array to modify
// int_1: element index
//
// ( hash_1 string_1 -- )
// hash_1: hash to modify
// string_1: key
//
// ( string_2 int_2 -- )
// string_2: string to modify
// int_2: element index 
//
// Read the respective element of array_1, hash_1, or string_2.
//
// example:
//
// [ 10 20 30 ] 2 get                   # 30
// ( "foo" 10 "bar" 20 ) "foo" get      # 10
// "ABC" 1 get                          # 66
//
void gfx_prim_get()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  if(!ptr1) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  obj_id_t val = 0;

  switch(ptr1->base_type) {
    case OTYPE_ARRAY:
      {
        int64_t *idx = gfx_obj_num_ptr(id2);
        if(!idx) {
          GFX_ERROR(err_invalid_range);
          return;
        }
        val = gfx_obj_array_get(id1, *idx);
        gfx_obj_ref_inc(val);
      }
      break;

    case OTYPE_HASH:
      {
        data_t *key = gfx_obj_mem_ptr(id2);
        if(!key) {
          GFX_ERROR(err_invalid_hash_key);
          return;
        }
        val = gfx_obj_hash_get(id1, key).id2;
        gfx_obj_ref_inc(val);
      }
      break;

    case OTYPE_MEM:
      {
        int64_t *idx = gfx_obj_num_ptr(id2);
        if(!idx) {
          GFX_ERROR(err_invalid_range);
          return;
        }
        int i = gfx_obj_mem_get(id1, *idx);
        if(i != -1) {
          val = gfx_obj_num_new(i, t_int);
        }
      }
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  // we did the ref counting already above
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, val, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_prim_get_x(data_t *key)
{
  arg_t *argv;

  argv = gfx_arg_1(OTYPE_HASH);

  if(argv) {
    obj_id_pair_t pair = gfx_obj_hash_get(argv[0].id, key);
    if(!pair.id1) {
      GFX_ERROR(err_invalid_arguments);
      return;
    }
    gfx_exec_id(argv[0].id, pair.id2, 1);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_prim_put_x(obj_id_t key)
{
  arg_t *argv;

  argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_HASH | IS_RW, OTYPE_ANY | IS_NIL });

  if(argv) {
    if(!gfx_obj_hash_set(argv[0].id, key, argv[1].id, 1)) {
      GFX_ERROR(err_invalid_hash_key);
      return;
    }

    gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set array, hash, or string element
//
// group: get
//
// ( array_1 int_1 any_1  -- )
// array_1: array to modify
// int_1: element index
// any_1: new value
//
// ( hash_1 string_1 any_2  -- )
// hash_1: hash to modify
// string_1: key
// any_2: new value
//
// ( string_2 int_2 int_3  -- )
// string_2: string to modify
// int_2: element index
// int_3: new value
//
// Set the respective element of array_1, hash_1, or string_2.
//
// Note that string constants are read-only and cannot be modified.
//
// example:
//
// /x [ 10 20 30 ] def
// x 2 40 put                           # x is now [ 10 20 40 ]
//
// /y ( "foo" 10 "bar" 20 ) def
// y "bar" 40 put                       # y is now ( "foo" 10 "bar" 40 )
//
// /z "ABC" mem def                     # mem is needed to create a writable copy
// z 1 68 put                           # z is now "ADC"
//
void gfx_prim_put()
{
  arg_t *argv;

  argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_ARRAY | IS_RW, OTYPE_NUM, OTYPE_ANY | IS_NIL });

  if(argv) {
    int pos = OBJ_VALUE_FROM_PTR(argv[1].ptr);

    if(!gfx_obj_array_set(argv[0].id, argv[2].id, pos, 1)) {
      GFX_ERROR(err_invalid_range);
      return;
    }

    gfx_obj_array_pop_n(3, gfxboot_data->vm.program.pstack, 1);
    return;
  }

  if(gfxboot_data->vm.error.id == err_readonly) return;

  argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_HASH | IS_RW, OTYPE_MEM, OTYPE_ANY | IS_NIL });

  if(argv) {
    if(!gfx_obj_hash_set(argv[0].id, argv[1].id, argv[2].id, 1)) {
      GFX_ERROR(err_invalid_hash_key);
      return;
    }

    gfx_obj_array_pop_n(3, gfxboot_data->vm.program.pstack, 1);
    return;
  }

  if(gfxboot_data->vm.error.id == err_readonly) return;

  argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_MEM | IS_RW, OTYPE_NUM, OTYPE_NUM });

  if(argv) {
    uint8_t val = OBJ_VALUE_FROM_PTR(argv[2].ptr);
    int pos = OBJ_VALUE_FROM_PTR(argv[1].ptr);

    if(!gfx_obj_mem_set(argv[0].id, val, pos)) {
      GFX_ERROR(err_invalid_range);
      return;
    }

    gfx_obj_array_pop_n(3, gfxboot_data->vm.program.pstack, 1);
    return;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set array, hash, or string element
//
// group: get
//
// ( array_1 int_1 any_1  -- )
// array_1: array to modify
// int_1: element index
// any_1: new value
//
// ( hash_1 string_1 any_2  -- )
// hash_1: hash to modify
// string_1: key
// any_2: new value
//
// ( string_2 int_2 int_3  -- )
// string_2: string to modify
// int_2: element index
// int_3: new value
//
// Insert an element into array_1, hash_1, or string_2 at the respective position.
//
// Note that string constants are read-only and cannot be modified.
//
// example:
//
// /x [ 10 20 30 ] def
// x 2 40 insert                        # x is now [ 10 20 40 30 ]
//
// /y ( "foo" 10 "bar" 20 ) def
// y "bar" 40 insert                    # y is now ( "foo" 10 "bar" 40 )
//
// /z "ABC" mem def                     # mem is needed to create a writable copy
// z 1 68 insert                        # z is now "ADBC"
//
void gfx_prim_insert()
{
  arg_t *argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_MEM | IS_RW, OTYPE_NUM, OTYPE_NUM });
  if(!argv) argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_HASH | IS_RW, OTYPE_MEM, OTYPE_ANY | IS_NIL });
  if(!argv) argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_ARRAY | IS_RW, OTYPE_NUM, OTYPE_ANY | IS_NIL });

  if(!argv) return;

  int pos;
  uint8_t val;

  switch(argv[0].ptr->base_type) {
    case OTYPE_MEM:
      val = OBJ_VALUE_FROM_PTR(argv[2].ptr);
      pos = OBJ_VALUE_FROM_PTR(argv[1].ptr);
      if(!gfx_obj_mem_insert(argv[0].id, val, pos)) {
        GFX_ERROR(err_invalid_range);
        return;
      }
      break;

    case OTYPE_ARRAY:
      pos = OBJ_VALUE_FROM_PTR(argv[1].ptr);
      if(!gfx_obj_array_insert(argv[0].id, argv[2].id, pos, 1)) {
        GFX_ERROR(err_invalid_range);
        return;
      }

      break;

    case OTYPE_HASH:
      if(!gfx_obj_hash_set(argv[0].id, argv[1].id, argv[2].id, 1)) {
        GFX_ERROR(err_invalid_hash_key);
        return;
      }
      break;
  }

  gfx_obj_array_pop_n(3, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// delete an array, hash, or string element
//
// group: get
//
// ( array_1 int_1 -- )
// array_1: array to modify
// int_1: element index
//
// ( hash_1 string_1 -- )
// hash_1: hash to modify
// string_1: key
//
// ( string_2 int_2 -- )
// string_2: string to modify
// int_2: element index 
//
// Delete the respective element of array_1, hash_1, or string_2. The length
// of array_1 andstring_2 will be reduced by 1.
//
// Note that string constants are read-only and cannot be modified.
//
// example:
//
// /x [ 10 20 30 ] def
// x 1 delete                           # x is now [ 10 30 ]
//
// /y ( "foo" 10 "bar" 20 ) def
// y "foo" delete                       # y is now ( "bar" 20 )
//
// /z "ABC" mem def                     # mem is needed to create a writable copy
// z 1 delete                           # z is now "AC"
//
void gfx_prim_delete()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  if(!ptr1) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  if(ptr1->flags.ro) {
    GFX_ERROR(err_readonly);
    return;
  }

  switch(ptr1->base_type) {
    case OTYPE_ARRAY:
      {
        int64_t *idx = gfx_obj_num_ptr(id2);
        if(!idx) {
          GFX_ERROR(err_invalid_range);
          return;
        }
        gfx_obj_array_del(id1, *idx, 1);
      }
      break;

    case OTYPE_HASH:
      {
        data_t *key = gfx_obj_mem_ptr(id2);
        if(!key) {
          GFX_ERROR(err_invalid_hash_key);
          return;
        }
        gfx_obj_hash_del(id1, id2, 1);
      }
      break;

    case OTYPE_MEM:
      {
        int64_t *idx = gfx_obj_num_ptr(id2);
        if(!idx) {
          GFX_ERROR(err_invalid_range);
          return;
        }
        gfx_obj_mem_del(id1, *idx);
      }
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get size of array, hash, or string
//
// group: get
//
// ( array_1 -- int_1 )
// int_1: number of elements in array_1
//
// ( hash_1 -- int_2 )
// int_2: number of key - value pairs in hash_1
//
// ( string_1 -- int_3 )
// int_3: number of bytes in string_1
//
// Put the length of array_1, hash_1, or string_1 on the stack.
//
// example:
//
// [ 10 20 30 ] length                  # 3
// ( "foo" 10 "bar" 20 ) length         # 2
// "ABC" length                         # 3
//
void gfx_prim_length()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  if(!ptr1) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  int64_t val = 0;

  switch(ptr1->base_type) {
    case OTYPE_ARRAY:
      val = OBJ_ARRAY_FROM_PTR(ptr1)->size;
      break;

    case OTYPE_HASH:
      val = OBJ_HASH_FROM_PTR(ptr1)->size;
      break;

    case OTYPE_MEM:
      val = OBJ_DATA_FROM_PTR(ptr1)->size;
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  obj_id_t val_id = gfx_obj_num_new(val, t_int);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  // we did the ref counting already above
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, val_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// duplicate TOS
//
// group: stack
//
// ( any_1 -- any_1 any_1 )
//
// Duplicate the top-of-stack element.
//
// example:
//
// 10 dup               # 10 10
//
void gfx_prim_dup()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, id1, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// remove TOS
//
// group: stack
//
// ( any_1 -- )
//
// Remove the top-of-stack element.
//
// example:
//
// 10 20 pop            # 10
//
void gfx_prim_pop()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// swap upper two stack elements
//
// group: stack
//
// ( any_1 any_2 -- any_2 any_1 )
//
// Swap the two topmost stack elements.
//
// example:
//
// 10 20 exch            # 20 10
//
void gfx_prim_exch()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  gfx_obj_array_set(gfxboot_data->vm.program.pstack, id2, (int) pstack->size - 2, 0);
  gfx_obj_array_set(gfxboot_data->vm.program.pstack, id1, (int) pstack->size - 1, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// rotate upper three stack elements
//
// group: stack
//
// ( any_1 any_2 any_3 -- any_2 any_3 any_1 )
//
// Rotate any_1 to the top-of-stack.
//
// example:
//
// 10 20 30 rot         # 20 30 10
//
void gfx_prim_rot()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 3) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 3];
  obj_id_t id2 = pstack->ptr[pstack->size - 2];
  obj_id_t id3 = pstack->ptr[pstack->size - 1];

  gfx_obj_array_set(gfxboot_data->vm.program.pstack, id2, (int) pstack->size - 3, 0);
  gfx_obj_array_set(gfxboot_data->vm.program.pstack, id3, (int) pstack->size - 2, 0);
  gfx_obj_array_set(gfxboot_data->vm.program.pstack, id1, (int) pstack->size - 1, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// rotate stack elements
//
// group: stack
//
// ( any_1 ... any_n int_1 int_2 -- any_x ... any_y )
// int_1: number of stack elements to rotate (equal to index n)
// int_2: rotation amount
//
// Rotate the n elements any_1 ... any_n. The new positions are calculated as follows:
//
// x = (1 - int_2) mod int_1
//
// y = (n - int_2) mod int_1
//
// This can be seen as rotating int_1 elements up by int_2 resp. down by -int_2.
//
// example:
//
// 10 20 30 40 50 5 2 roll      # 40 50 10 20 30
//
// /rot { 3 -1 roll } def       # definition of 'rot'
//
void gfx_prim_roll()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  int64_t *xlen = gfx_obj_num_ptr(id1);
  int64_t *xofs = gfx_obj_num_ptr(id2);

  if(!xlen || *xlen < 0 || !xofs) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  int len = *xlen;
  int ofs = *xofs;

  if((unsigned) len + 2 > pstack->size) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  if(!len) return;

  ofs %= len;
  if(ofs < 0) ofs += len;
  if(!ofs) return;

  ofs = len - ofs;

  // FIXME: it's a bit inefficient this way
  while(ofs--) {
    obj_id_t tmp_id = pstack->ptr[(int) pstack->size - len];
    for(int i = 0; i < len - 1; i++) {
      pstack->ptr[(int) pstack->size - len + i] = pstack->ptr[(int) pstack->size - len + i + 1];
    }
    pstack->ptr[pstack->size - 1] = tmp_id;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// copy TOS-1 to TOS
//
// group: stack
//
// ( any_1 any_2 -- any_1 any_2 any_1 )
//
// Put a copy of the second-from-top element on the top-of-stack.
//
// example:
//
// 10 20 over           # 10 20 10
//
void gfx_prim_over()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, id1, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// copy stack element
//
// group: stack
//
// ( any_n ... any_0 int_1 -- any_n ... any_0 any_n )
// int_1: element position on stack (n is equal to int_1)
//
// Copy the int_1-th-from-top element on the top-of-stack.
//
// example:
//
// 10 20 30 40 3 index          # 10 20 30 40 10
//
// /dup { 0 index } def         # definition of 'dup'
//
// /over { 1 index } def        # definition of 'over'
//
void gfx_prim_index()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];
  int64_t *idx = gfx_obj_num_ptr(id1);

  if(!idx || *idx < 0) {
    GFX_ERROR(err_invalid_range);
    return;
  }

  if(*idx + 2 > pstack->size) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id2 = pstack->ptr[pstack->size - 2 - *idx];

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, id2, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_exec_id(obj_id_t dict_id, obj_id_t id, int on_stack)
{
  int64_t *val;

  if((val = gfx_obj_num_subtype_ptr(id, t_prim))) {
    if(on_stack) gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
    gfx_run_prim(*val);
  }
  else if(gfx_obj_mem_subtype_ptr(id, t_code)) {
    obj_id_t context_id = gfx_obj_context_new(t_ctx_func);
    context_t *context = gfx_obj_context_ptr(context_id);
    if(!context) {
      GFX_ERROR(err_no_memory);
      return;
    }
    context->code_id = gfx_obj_ref_inc(id);

    context->parent_id = gfxboot_data->vm.program.context;
    gfxboot_data->vm.program.context = context_id;

    if(dict_id) {
      context->dict_id = gfx_obj_hash_new(0);
      hash_t *hash = gfx_obj_hash_ptr(context->dict_id);
      if(!hash) {
        GFX_ERROR(err_no_memory);
        return;
      }
      hash->parent_id = gfx_obj_ref_inc(dict_id);
    }

    if(on_stack) gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  }
  else {
    gfx_obj_ref_inc(id);
    if(on_stack) gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
    gfx_obj_array_push(gfxboot_data->vm.program.pstack, id, 0);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// execute object
//
// group: loop
//
// ( ref_1 -- )
// ref_1: word reference
//
// ( code_1 -- )
// code_1: code block
//
// Executes the given code block or looks up and executes the word reference.
//
// example:
//
// { 10 20 } exec                       # 10 20
//
// /foo "abc" def
// foo                                  # "abc"
// /foo exec                            # "abc"
//
void gfx_prim_exec()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id = pstack->ptr[pstack->size - 1];
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr) return;

  if(
    ptr->base_type == OTYPE_MEM &&
    (ptr->sub_type == t_word || ptr->sub_type == t_ref)
  ) {
    id = gfx_lookup_dict(OBJ_DATA_FROM_PTR(ptr)).id2;
  }

  gfx_exec_id(0, id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// addition
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 + int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 xor bool_2
//
// ( array_1 array_2 -- array_3 )
// array_3: array_2 appended to array_1
//
// ( hash_1 hash_2 -- hash_3 )
// hash_3: joined hash_1 and hash_2
//
// ( string_1 string_2 -- string_3 )
// string_3: string_2 appended to string_1
//
// Add two numbers, or concatenate two arrays, or join two hashes, or concatenate two strings.
//
// For boolean 1 bit arithmetic this is equivalent to 'xor'.
//
// example:
//
// 10 20 add                            # 30
// true true add                        # false
// [ 10 20 ] [ 30 40 ] add              # [ 10 20 30 40 ]
// ( "foo" 10 ) ( "bar" 20 ) add        # ( "bar" 20 "foo" 10 )
// "abc" "xyz" add                      # "abcxyz"
//
void gfx_prim_add()
{
  gfx_prim__add(0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// addition
//
// group: calc
//
// ( ref_1 obj_1 -- )
//
// Add obj_1 to the object ref_1 references.
//
// The same type of object combinations as for add are allowed.
// But add! modifies the object ref_1 references directly.
// Also, the result is not put on the stack.
//
// example:
//
// /foo 10 def
// /foo 20 add                          # foo is 30
//
// /bar "abc" def
// /bar "xyz" add                       # bar is "abcxyz"
//
void gfx_prim_add_direct()
{
  gfx_prim__add(1);
}


void gfx_prim__add(unsigned direct)
{
  arg_t *argv;

  argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_ANY, OTYPE_ANY });

  if(!argv) return;

  obj_id_t id1 = argv[0].id;
  obj_id_t id2 = argv[1].id;

  obj_id_t direct_dict = 0;
  obj_id_t direct_key = argv[0].id;

  obj_t *ptr1 = argv[0].ptr;
  obj_t *ptr2 = argv[1].ptr;

  if(direct) {
    if(!ptr1 || ptr1->base_type != OTYPE_MEM || ptr1->sub_type != t_ref) {
      GFX_ERROR(err_invalid_arguments);
      return;
    }
    obj_id_pair_t pair = gfx_lookup_dict(OBJ_DATA_FROM_PTR(ptr1));

    if(!pair.id1) {
      GFX_ERROR(err_invalid_hash_key);
      return;
    }

    direct_dict = pair.id1;
    argv[0].id = pair.id2;

    OBJ_PTR_UPDATE(argv[0]);

    id1 = argv[0].id;
    ptr1 = argv[0].ptr;
  }

  if(!ptr1 || !ptr2 || ptr1->base_type != ptr2->base_type) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  if(direct && ptr1->flags.ro) {
    GFX_ERROR(err_readonly);
    return;
  }

  obj_id_t result_id = 0;

  switch(ptr1->base_type) {
    case OTYPE_NUM:
      {
        int64_t result = ptr1->data.value + ptr2->data.value;
        if(ptr1->sub_type == t_bool) result &= 1;
        if(direct) {
          ptr1->data.value = result;
          result_id = gfx_obj_ref_inc(id1);
        }
        else {
          result_id = gfx_obj_num_new(result, ptr1->sub_type);
        }
      }
      break;

    case OTYPE_MEM:
      {
        unsigned new_size = ptr1->data.size + ptr2->data.size;
        unsigned part1_size = ptr1->data.size;
        if(direct) {
          result_id = new_size > part1_size ? gfx_obj_realloc(id1, new_size) : id1;
          gfx_obj_ref_inc(result_id);
        }
        else {
          result_id = gfx_obj_mem_new(new_size, 0);
        }
        obj_t *result_ptr = gfx_obj_ptr(result_id);
        if(!result_ptr) {
          GFX_ERROR(err_no_memory);
          return;
        }
        if(!direct) {
          result_ptr->sub_type = ptr1->sub_type;
          gfx_memcpy(result_ptr->data.ptr, ptr1->data.ptr, part1_size);
        }
        gfx_memcpy(result_ptr->data.ptr + part1_size, ptr2->data.ptr, ptr2->data.size);
      }
      break;

    case OTYPE_ARRAY:
      {
        array_t *array1 = gfx_obj_array_ptr(id1);
        array_t *array2 = gfx_obj_array_ptr(id2);
        if(direct) {
          result_id = gfx_obj_ref_inc(id1);
        }
        else if(array1 && array2) {
          result_id = gfx_obj_array_new(array1->size + array2->size + 0x10);
        }
        if(!result_id) {
          GFX_ERROR(err_no_memory);
          return;
        }
        obj_id_t val;
        unsigned idx = 0;
        if(!direct) {
          while(gfx_obj_iterate(id1, &idx, &val, 0)) {
            // note: reference counting for val has been done inside gfx_obj_iterate()
            gfx_obj_array_push(result_id, val, 0);
          }
        }
        idx = 0;
        while(gfx_obj_iterate(id2, &idx, &val, 0)) {
          // note: reference counting for val has been done inside gfx_obj_iterate()
          gfx_obj_array_push(result_id, val, 0);
        }
      }
      break;

    case OTYPE_HASH:
      {
        hash_t *hash1 = gfx_obj_hash_ptr(id1);
        hash_t *hash2 = gfx_obj_hash_ptr(id2);
        if(direct) {
          result_id = gfx_obj_ref_inc(id1);
        }
        else if(hash1 && hash2) {
          result_id = gfx_obj_hash_new(hash1->size + hash2->size + 0x10);
        }
        if(!result_id) {
          GFX_ERROR(err_no_memory);
          return;
        }
        obj_id_t key, val;
        unsigned idx = 0;
        if(!direct) {
          while(gfx_obj_iterate(id1, &idx, &key, &val)) {
            // note: reference counting for key & val has been done inside gfx_obj_iterate()
            gfx_obj_hash_set(result_id, key, val, 0);
          }
        }
        idx = 0;
        while(gfx_obj_iterate(id2, &idx, &key, &val)) {
          // note: reference counting for key & val has been done inside gfx_obj_iterate()
          gfx_obj_hash_set(result_id, key, val, 0);
        }
      }
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  if(direct) {
    gfx_obj_hash_set(direct_dict, direct_key, result_id, 1);
    gfx_obj_ref_dec(result_id);
  }
  else {
    gfx_obj_array_push(gfxboot_data->vm.program.pstack, result_id, 0);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// subtraction
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 - int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 xor bool_2
//
// Subtract int_2 from int_1.
//
// For boolean 1 bit arithmetic this is equivalent to 'xor'.
//
// example:
//
// 100 30 sub                   # 70
// false true sub               # true
//
void gfx_prim_sub()
{
  binary_op_on_stack(op_sub);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// multiplication
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 * int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 and bool_2
//
// Multiply int_1 by int_2.
//
// For boolean 1 bit arithmetic this is equivalent to 'and'.
//
// example:
//
// 20 30 mul                    # 600
// true false mul               # false
//
void gfx_prim_mul()
{
  binary_op_on_stack(op_mul);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// division
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 / int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 / bool_2
//
// Divide int_1 by int_2.
//
// You can do a 1 bit division with boolean values. Note that this will run
// into a division by zero exception if bool_2 is false.
//
// example:
//
// 200 30 div                   # 6
// true true div                # true
//
void gfx_prim_div()
{
  binary_op_on_stack(op_div);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// remainder
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 % int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 / bool_2
//
// int_3 is the remainder dividing int_1 by int_2.
//
// You can get the remainder from a 1 bit division with boolean values. Note
// that this will run into a division by zero exception if bool_2 is false.
//
// example:
//
// 200 30 mod                   # 20
// true true mod                # false
//
void gfx_prim_mod()
{
  binary_op_on_stack(op_mod);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// negation
//
// group: calc
//
// ( int_1 -- int_2 )
// int_2: -int_1
//
// ( bool_1 -- bool_2 )
// bool_2: -bool_1
//
// Negate int_1 (change sign).
//
// For boolean 1 bit arithmetic the value is unchanged (this is not a 'not' operation).
//
// example:
//
// 20 neg                       # -20
// true neg                     # true
//
void gfx_prim_neg()
{
  unary_op_on_stack(op_neg);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// absolute value
//
// group: calc
//
// ( int_1 -- int_2 )
// int_2: |int_1|
//
// ( bool_1 -- bool_2 )
// bool_2: bool_1
//
// Absolute value of int_1 (change sign if int_1 is negative).
//
// example:
//
// For boolean 1 bit arithmetic the value is unchanged.
//
// -20 abs                      # 20
// true abs                     # true
//
void gfx_prim_abs()
{
  unary_op_on_stack(op_abs);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// minimum
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: minimum(int_1, int_2)
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 and bool_2
//
// int_3 is the smaller value of int_1 and int_2.
//
// For boolean 1 bit arithmetic this is equivalent to 'and'
//
// example:
//
// 10 20 min                    # 10
// true false min               # false
//
void gfx_prim_min()
{
  binary_op_on_stack(op_min);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// maximum
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: maximum(int_1, int_2)
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 or bool_2
//
// int_3 is the larger value of int_1 and int_2.
//
// For boolean 1 bit arithmetic this is equivalent to 'or'
//
// example:
//
// 10 20 max                    # 20
// true false max               # true
//
void gfx_prim_max()
{
  binary_op_on_stack(op_max);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// and
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 and int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 and bool_2
//
// example:
//
// 15 4 and                     # 4
// true false and               # false
//
void gfx_prim_and()
{
  binary_op_on_stack(op_and);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// or
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 or int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 or bool_2
//
// example:
//
// 15 4 or                      # 15
// true false or                # true
//
void gfx_prim_or()
{
  binary_op_on_stack(op_or);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// exclusive or
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 xor int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 xor bool_2
//
// example:
//
// 15 4 xor                     # 11
// true false or                # true
//
void gfx_prim_xor()
{
  binary_op_on_stack(op_xor);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// not
//
// group: calc
//
// ( int_1 -- int_2 )
// int_2: -int_1 - 1
//
// ( bool_1 -- bool_2 )
// bool_2: !bool_1
//
// example:
//
// 20 not                       # -21
// true not                     # false
//
void gfx_prim_not()
{
  unary_op_on_stack(op_not);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// shift left
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 << int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 and !bool_2
//
// example:
//
// 1 4 shl                      # 16
// true false shl               # true
//
void gfx_prim_shl()
{
  binary_op_on_stack(op_shl);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// shift right
//
// group: calc
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 >> int_2
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 and !bool_2
//
// example:
//
// 16 4 shr                     # 1
// true false shr               # true
//
void gfx_prim_shr()
{
  binary_op_on_stack(op_shr);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// equal
//
// group: cmp
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 == bool_2
//
// ( int_1 int_2 -- bool_4 )
// bool_4: int_1 == int_2
//
// ( string_1 string_2 -- bool_5 )
// bool_5: string_1 == string_2
//
// ( any_1 any_2 -- bool_6 )
// bool_6: any_1 == any_2
//
// For pairs of booleans, integers, and strings the values are compared. For all
// other combinations the internal object id is compared.
//
// example:
//
// 10 20 eq                     # false
// true false eq                # false
// "abc" "abc" eq               # true
// [ 10 20 ] [ 10 20 ] eq       # false
// 0 false eq                   # false
// 0 nil eq                     # false
// "abc" [ 10 ] eq              # false
//
// /foo [ 10 20 ] def
// /bar foo def
// foo bar eq                   # true
//
void gfx_prim_eq()
{
  binary_cmp_on_stack(op_eq);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// not equal
//
// group: cmp
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 != bool_2
//
// ( int_1 int_2 -- bool_4 )
// bool_4: int_1 != int_2
//
// ( string_1 string_2 -- bool_5 )
// bool_5: string_1 != string_2
//
// ( any_1 any_2 -- bool_6 )
// bool_6: any_1 != any_2
//
// For pairs of booleans, integers, and strings the values are compared. For all
// other combinations the internal object id is compared.
//
// example:
//
// 10 20 ne                     # true
// true false ne                # true
// "abc" "abc" ne               # false
// [ 10 20 ] [ 10 20 ] ne       # true
// 0 false ne                   # true
// 0 nil ne                     # true
// "abc" [ 10 ] ne              # true
//
// /foo [ 10 20 ] def
// /bar foo def
// foo bar ne                   # false
//
void gfx_prim_ne()
{
  binary_cmp_on_stack(op_ne);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// greater than
//
// group: cmp
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 > bool_2
//
// ( int_1 int_2 -- bool_4 )
// bool_4: int_1 > int_2
//
// ( string_1 string_2 -- bool_5 )
// bool_5: string_1 > string_2
//
// ( any_1 any_2 -- bool_6 )
// bool_6: any_1 > any_2
//
// For pairs of booleans, integers, and strings the values are compared. For all
// other combinations the internal object id is compared.
//
// example:
//
// 10 20 gt                     # false
// true false gt                # true
// "abd" "abc" gt               # true
// [ 10 20 ] [ 10 20 ] gt       # varies
// 0 false gt                   # varies
// 0 nil gt                     # varies
// "abc" [ 10 ] gt              # varies
//
void gfx_prim_gt()
{
  binary_cmp_on_stack(op_gt);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// greater or equal
//
// group: cmp
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 >= bool_2
//
// ( int_1 int_2 -- bool_4 )
// bool_4: int_1 >= int_2
//
// ( string_1 string_2 -- bool_5 )
// bool_5: string_1 >= string_2
//
// ( any_1 any_2 -- bool_6 )
// bool_6: any_1 >= any_2
//
// For pairs of booleans, integers, and strings the values are compared. For all
// other combinations the internal object id is compared.
//
// example:
//
// 10 20 ge                     # false
// true false ge                # true
// "abd" "abc" ge               # true
// [ 10 20 ] [ 10 20 ] ge       # varies
// 0 false ge                   # varies
// 0 nil ge                     # varies
// "abc" [ 10 ] ge              # varies
//
void gfx_prim_ge()
{
  binary_cmp_on_stack(op_ge);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// less than
//
// group: cmp
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 < bool_2
//
// ( int_1 int_2 -- bool_4 )
// bool_4: int_1 < int_2
//
// ( string_1 string_2 -- bool_5 )
// bool_5: string_1 < string_2
//
// ( any_1 any_2 -- bool_6 )
// bool_6: any_1 < any_2
//
// For pairs of booleans, integers, and strings the values are compared. For all
// other combinations the internal object id is compared.
//
// example:
//
// 10 20 lt                     # true
// true false lt                # false
// "abd" "abc" lt               # false
// [ 10 20 ] [ 10 20 ] lt       # varies
// 0 false lt                   # varies
// 0 nil lt                     # varies
// "abc" [ 10 ] lt              # varies
//
void gfx_prim_lt()
{
  binary_cmp_on_stack(op_lt);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// less or equal
//
// group: cmp
//
// ( bool_1 bool_2 -- bool_3 )
// bool_3: bool_1 <= bool_2
//
// ( int_1 int_2 -- bool_4 )
// bool_4: int_1 <= int_2
//
// ( string_1 string_2 -- bool_5 )
// bool_5: string_1 <= string_2
//
// ( any_1 any_2 -- bool_6 )
// bool_6: any_1 <= any_2
//
// For pairs of booleans, integers, and strings the values are compared. For all
// other combinations the internal object id is compared.
//
// example:
//
// 10 20 le                     # true
// true false le                # false
// "abd" "abc" le               # false
// [ 10 20 ] [ 10 20 ] le       # varies
// 0 false le                   # varies
// 0 nil le                     # varies
// "abc" [ 10 ] le              # varies
//
void gfx_prim_le()
{
  binary_cmp_on_stack(op_le);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// compare
//
// group: cmp
//
// ( int_1 int_2 -- int_3 )
// int_3: int_1 <=> int_2
//
// ( bool_1 bool_2 -- int_4 )
// int_4: bool_1 <=> bool_2
//
// ( string_1 string_2 -- int_5 )
// int_5: string_1 <=> string_2
//
// ( any_1 any_2 -- int_6 )
// int_6: any_1 <=> any_2
//
// For pairs of booleans, integers, and strings the values are compared. For
// all other combinations the internal object id is compared.
//
// The result is -1, 1, 0 if the first argument is less than, greater than,
// or equal to the second argument, respectively.
//
// example:
//
// 10 20 cmp                    # -1
// true false cmp               # 1
// "abc" "abc" cmp              # 0
// [ 10 20 ] [ 10 20 ] cmp      # varies
// 0 false cmp                  # varies
// 0 nil cmp                    # varies
// "abc" [ 10 ] cmp             # varies
//
// /foo [ 10 20 ] def
// /bar foo def
// foo bar cmp                  # 0
//
void gfx_prim_cmp()
{
  binary_cmp_on_stack(op_cmp);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int is_true(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);
  int val = 0;

  if(ptr) {
    if(ptr->base_type == OTYPE_NUM) {
      val = ptr->data.value ? 1 : 0;
    }
    else {
      val = 1;
    }
  }

  return val;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
error_id_t do_op(op_t op, int64_t *result, int64_t val1, int64_t val2, unsigned sub_type)
{
  error_id_t err = 0;

  switch(op) {
    case op_sub:
      *result = val1 - val2;
      break;

    case op_mul:
      if(sub_type == t_bool) {
        *result = (val1 & val2) & 1;
      }
      else {
        *result = val1 * val2;
      }
      break;

    case op_div:
      if(sub_type == t_bool) {
        int i1 = val1 & 1;
        int i2 = val2 & 1;
        if(i2 == 0) {
          err = err_div_by_zero;
        }
        else {
          *result = i1;
        }
      }
      else {
        if(
          val2 == 0 ||
          ((uint64_t) val1 == 0x8000000000000000ll && val2 == -1)
        ) {
          err = err_div_by_zero;
        }
        else {
          *result = val1 / val2;
        }
      }
      break;

    case op_mod:
      if(sub_type == t_bool) {
        int i1 = val1 & 1;
        int i2 = val2 & 1;
        if(i2 == 0) {
          err = err_div_by_zero;
        }
        else {
          *result = i1;
        }
      }
      else {
        if(
          val2 == 0 ||
          ((uint64_t) val1 == 0x8000000000000000ll && val2 == -1)
        ) {
          err = err_div_by_zero;
        }
        else {
          *result = val1 % val2;
        }
      }
      break;

    case op_and:
      *result = val1 & val2;
      break;

    case op_or:
      *result = val1 | val2;
      break;

    case op_xor:
      *result = val1 ^ val2;
      break;

    case op_min:
      *result = val1 < val2 ? val1 : val2;
      break;

    case op_max:
      *result = val1 > val2 ? val1 : val2;
      break;

    case op_shl:
      *result = val1 << val2;
      break;

    case op_shr:
      *result = val1 >> val2;
      break;

    case op_neg:
      *result = -val1;
      break;

    case op_not:
      *result = ~val1;
      break;

    case op_abs:
      *result = val1 > 0 ? val1 : -val1;
      break;

    default:
      *result = 0;
      break;
  }

  if(sub_type == t_bool) *result &= 1;

  return err;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void binary_op_on_stack(op_t op)
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  obj_t *ptr2 = gfx_obj_ptr(id2);

  if(!ptr1 || !ptr2 || ptr1->base_type != ptr2->base_type || ptr1->base_type != OTYPE_NUM) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  int64_t result;

  error_id_t err = do_op(op, &result, ptr1->data.value, ptr2->data.value, ptr1->sub_type);

  if(err) {
    GFX_ERROR(err);
    return;
  }

  obj_id_t result_id = gfx_obj_num_new(result, ptr1->sub_type);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, result_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void unary_op_on_stack(op_t op)
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id = pstack->ptr[pstack->size - 1];
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_NUM) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  int64_t result;

  error_id_t err = do_op(op, &result, ptr->data.value, 0, ptr->sub_type);

  if(err) {
    GFX_ERROR(err);
    return;
  }

  obj_id_t result_id = gfx_obj_num_new(result, ptr->sub_type);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, result_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void binary_cmp_on_stack(cmp_op_t op)
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  obj_t *ptr2 = gfx_obj_ptr(id2);

  int result = 0;

  if(id1 != 0 || id2 != 0) {
    if(!ptr1 || !ptr2) {
      result = id1 > id2 ? 1 : -1;
    }
    else if(ptr1->base_type != ptr2->base_type) {
      result = ptr1->base_type > ptr2->base_type ? 1 : -1;
    }
    else {
      switch(ptr1->base_type) {
        case OTYPE_NUM:
          if(ptr1->sub_type != ptr2->sub_type) {
            result = ptr1->sub_type > ptr2->sub_type ? 1 : -1;
          }
          else {
            if(ptr1->data.value != ptr2->data.value) {
              result = ptr1->data.value > ptr2->data.value ? 1 : -1;
            }
          }
          break;

        case OTYPE_MEM:
          if(ptr1->sub_type != ptr2->sub_type) {
            result = ptr1->sub_type > ptr2->sub_type ? 1 : -1;
          }
          else {
            result = gfx_obj_mem_cmp(&ptr1->data, &ptr2->data);
          }
          break;

        default:
          if(id1 != id2) {
            result = id1 > id2 ? 1 : -1;
          }
          break;
      }
    }
  }

  uint8_t subtype = t_bool;

  switch(op) {
    case op_eq:
      result = result == 0 ? 1 : 0;
      break;

    case op_ne:
      result = result != 0 ? 1 : 0;
      break;

    case op_gt:
      result = result > 0 ? 1 : 0;
      break;

    case op_ge:
      result = result >= 0 ? 1 : 0;
      break;

    case op_lt:
      result = result < 0 ? 1 : 0;
      break;

    case op_le:
      result = result <= 0 ? 1 : 0;
      break;

    case op_cmp:
      subtype = t_int;
      break;
  }

  obj_id_t result_id = gfx_obj_num_new(result, subtype);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, result_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get parent of context, font, or hash
//
// group: hash
//
// ( context_1 -- context_2 )
// context_2: parent of context_1 or nil
//
// ( font_1 -- font_2 )
// font_2: parent of font_1 or nil
//
// ( hash_1 -- hash_2 )
// hash_2: parent of hash_1 or nil
//
// If a word lookup fails in a context, the lookup continues in the parent
// context.
//
// If a glyph lookup fails in a font, the lookup continues in the parent
// font.
//
// If a key cannot be found in a hash, the lookup continues in the parent
// hash.
//
// example:
//
// /x ( "foo" 10 "bar" 20 ) def
// /y ( "zap" 30 ) def
// x getparent                          # nil
// x y setparent
// x getparent                          # ( "zap" 30 )
//
void gfx_prim_getparent()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);
  if(!ptr1) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  obj_id_t parent_id = 0;

  switch(ptr1->base_type) {
    case OTYPE_HASH:
      ;
      hash_t *hash = OBJ_HASH_FROM_PTR(ptr1);
      parent_id = hash->parent_id;
      break;

    case OTYPE_FONT:
      ;
      font_t *font = OBJ_FONT_FROM_PTR(ptr1);
      parent_id = font->parent_id;
      break;

    case OTYPE_CONTEXT:
      ;
      context_t *ctx = OBJ_CONTEXT_FROM_PTR(ptr1);
      parent_id = ctx->parent_id;
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, parent_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set parent of context, font, or hash
//
// group: hash
//
// ( context_1 context_2 -- )
// ( context_1 nil -- )
// context_2: new parent of context_1
//
// ( font_1 font_2 -- )
// ( font_1 nil -- )
// font_2: new parent of font_1
//
// ( hash_1 hash_2 -- )
// ( hash_1 nil -- )
// hash_2: new parent of hash_1
//
// If nil is used as second argument, any existing parent link is removed.
//
// If a word lookup fails in a context, the lookup continues in the parent
// context.
//
// If a glyph lookup fails in a font, the lookup continues in the parent
// font.
//
// If a key cannot be found in a hash, the lookup continues in the parent
// hash.
//
// example:
//
// /x ( "foo" 10 "bar" 20 ) def
// /y ( "zap" 30 ) def
// x "zap" get                          # nil
// x y setparent
// x "zap" get                          # 30
// x nil setparent
// x "zap" get                          # nil
//
void gfx_prim_setparent()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 2) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 2];
  obj_id_t id2 = pstack->ptr[pstack->size - 1];

  obj_t *ptr1 = gfx_obj_ptr(id1);

  if(!ptr1) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  if(ptr1->flags.ro) {
    GFX_ERROR(err_readonly);
    return;
  }

  obj_id_t old_parent_id = 0;

  switch(ptr1->base_type) {
    case OTYPE_HASH:
      if(id2 && !gfx_obj_hash_ptr(id2)) {
        GFX_ERROR(err_invalid_arguments);
        return;
      }
      hash_t *hash = OBJ_HASH_FROM_PTR(ptr1);
      old_parent_id = hash->parent_id;
      hash->parent_id = gfx_obj_ref_inc(id2);
      break;

    case OTYPE_FONT:
      if(id2 && !gfx_obj_font_ptr(id2)) {
        GFX_ERROR(err_invalid_arguments);
        return;
      }
      font_t *font = OBJ_FONT_FROM_PTR(ptr1);
      old_parent_id = font->parent_id;
      font->parent_id = gfx_obj_ref_inc(id2);
      break;

    case OTYPE_CONTEXT:
      if(id2 && !gfx_obj_context_ptr(id2)) {
        GFX_ERROR(err_invalid_arguments);
        return;
      }
      context_t *ctx = OBJ_CONTEXT_FROM_PTR(ptr1);
      old_parent_id = ctx->parent_id;
      ctx->parent_id = gfx_obj_ref_inc(id2);
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_ref_dec(old_parent_id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get active dictionary
//
// group: hash
//
// ( -- hash_1 )
// ( -- nil )
// hash_1: dictionary
//
// Return the currently active dictionary or nil, if the current context
// does not (yet) have a dictionary.
//
// A dictionary will only be created on demand - that is, the first time a
// word is defined in the current context.
//
// When a program is started the global context is created containing a
// dictionary with all primitive words.
//
// example:
//
// /foo { getdict } def
// foo                                  # nil
//
// /bar { /x 10 ldef getdict } def
// bar                                  # ( /x 10 )
//
void gfx_prim_getdict()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);
  if(!context) {
    GFX_ERROR(err_internal);
    return;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, context->dict_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set active dictionary
//
// group: hash
//
// ( hash_1 -- )
// ( nil -- )
// hash_1: new active dictionary
//
// Set the currently active dictionary. With nil, the dictionary is removed
// from the current context.
//
// example:
//
// /foo { /x 10 ldef x } def
// foo                                  # 10
//
// /bar { ( /x 10 ) setdict x } def
// bar                                  # 10
//
void gfx_prim_setdict()
{
  array_t *pstack = gfx_obj_array_ptr(gfxboot_data->vm.program.pstack);

  if(!pstack || pstack->size < 1) {
    GFX_ERROR(err_stack_underflow);
    return;
  }

  obj_id_t id1 = pstack->ptr[pstack->size - 1];

  if(id1 && !gfx_obj_hash_ptr(id1)) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);
  if(!context) {
    GFX_ERROR(err_internal);
    return;
  }

  OBJ_ID_ASSIGN(context->dict_id, id1);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// make object read-only
//
// group: mem
//
// ( any_1 -- any_1 )
//
// Make any object read-only. A read-only object cannot be modified.
//
// Note that string constants are read-only by default.
//
// example:
//
// [ 10 20 30 ] freeze                  # [ 10 20 30 ]
// 0 delete                             # raises 'readonly' exception
//
void gfx_prim_freeze()
{
  arg_t *argv = gfx_arg_1(OTYPE_ANY | IS_NIL);

  if(!argv) return;

  if(argv[0].ptr) argv[0].ptr->flags.ro = 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// make hash sticky
//
// group: mem
//
// ( hash_1 -- hash_1 )
//
// Mark hash as sticky.
//
// example:
//
// getdict sticky
//
void gfx_prim_sticky()
{
  arg_t *argv = gfx_arg_1(OTYPE_HASH);

  if(!argv) return;

  if(argv[0].ptr) argv[0].ptr->flags.sticky = 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get drawing color
//
// group: gfx
//
// ( -- int_1 )
// int_1: color
//
// Return current drawing color.
//
// A color is a RGB value with red in bits 16-23, green in bits 8-15 and
// blue in bits 0-7. This is independent of what the graphics card is actually using.
//
// example:
//
// getcolor                             # 0xffffff (white)
//
void gfx_prim_getcolor()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  int64_t col = 0;

  if(canvas) {
    col = canvas->color;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(col, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set drawing color
//
// group: gfx
//
// ( int_1 -- )
// int_1: color
//
// Set current drawing color.
//
// A color is a RGB value with red in bits 16-23, green in bits 8-15 and
// blue in bits 0-7. This is independent of what the graphics card is actually using.
//
// example:
//
// 0xff0000 setcolor                    # red
//
void gfx_prim_setcolor()
{
  arg_t *argv = gfx_arg_1(OTYPE_NUM);

  if(!argv) return;

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) canvas->color = OBJ_VALUE_FROM_PTR(argv[0].ptr);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get background color
//
// group: gfx
//
// ( -- int_1 )
// int_1: color
//
// Return current background color.
//
// A color is a RGB value with red in bits 16-23, green in bits 8-15 and
// blue in bits 0-7. This is independent of what the graphics card is actually using.
//
// example:
//
// getcolor                             # 0 (black)
//
void gfx_prim_getbgcolor()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  int64_t col = 0;

  if(canvas) {
    col = canvas->bg_color;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(col, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set background color
//
// group: gfx
//
// ( int_1 -- )
// int_1: color
//
// Set current background color.
//
// A color is a RGB value with red in bits 16-23, green in bits 8-15 and
// blue in bits 0-7. This is independent of what the graphics card is actually using.
//
// example:
//
// 0xff00 setcolor                      # green
//
void gfx_prim_setbgcolor()
{
  arg_t *argv = gfx_arg_1(OTYPE_NUM);

  if(!argv) return;

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) canvas->bg_color = OBJ_VALUE_FROM_PTR(argv[0].ptr);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get drawing position
//
// group: gfx
//
// ( -- int_1 int_2 )
// int_1: x
// int_2: y
// 
// Return current drawing position. The position is relative to the drawing region in the graphics state.
//
// example:
//
// getpos                               # 0 0
//
void gfx_prim_getpos()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  int64_t x = 0, y = 0;

  if(canvas) {
    x = canvas->cursor.x;
    y = canvas->cursor.y;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(x, t_int), 0);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(y, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set drawing position
//
// group: gfx
//
// ( int_1 int_2 -- )
// int_1: x
// int_2: y
// 
// Set drawing position. The position is relative to the drawing region in the graphics state.
//
// example:
//
// 20 30 setpos
//
void gfx_prim_setpos()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val0 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[1].ptr);

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    canvas->cursor.x = val0;
    canvas->cursor.y = val1;
  }

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get font
//
// group: gfx
//
// ( -- font_1 )
// ( -- nil )
// font_1: font
//
// Get current font.
//
// example:
//
// # get currently used font
//
// getfont
//
void gfx_prim_getfont()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  obj_id_t font_id = canvas ? canvas->font_id : 0;

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, font_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set font
//
// group: gfx
//
// ( font_1 -- )
// ( nil -- )
// font_1: font
//
// Set font. If nil is passed, no active font will be associated with the current canvas.
//
// example:
//
// # read font from file and use it
//
// "foo.fnt" readfile newfont setfont
//
void gfx_prim_setfont()
{
  arg_t *argv = gfx_arg_1(OTYPE_FONT | IS_NIL);

  if(!argv) return;

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    OBJ_ID_ASSIGN(canvas->font_id, argv[0].id);
    area_t area = gfx_font_dim(canvas->font_id);
    canvas->cursor.width = area.width;
    canvas->cursor.height = area.y;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// create font object
//
// group: gfx
//
// ( string_1 -- font_1 )
// ( string_1 -- nil )
// string_1: font data
// font_1: font object
//
// Parse font data in string_1 and create font object. If string_1 does not
// contain valid font data, return nil.
//
// example:
//
// /foo_font "foo.fnt" readfile newfont def     # create font from file "foo.fnt"
//
void gfx_prim_newfont()
{
  arg_t *argv = gfx_arg_1(OTYPE_MEM);

  if(!argv) return;

  obj_id_t font_id = gfx_obj_font_open(argv[0].id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, font_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get drawing mode
//
// group: gfx
//
// ( -- int_1 )
// int_1: drawing mode
//
// Return drawing mode of current canvas.
//
// Drawing mode is either 0 (merge mode) or 1 (direct mode).
//
// example:
//
// getdrawmode                             # 0 ('merge' mode)
//
void gfx_prim_getdrawmode()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  int64_t mode = 0;

  if(canvas) {
    mode = canvas->draw_mode;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(mode, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set drawing mode
//
// group: gfx
//
// ( int_1 -- )
// int_1: drawing mode
//
// Set drawing mode of current canvas.
//
// Drawing mode is either 0 (merge mode) or 1 (direct mode).
//
// example:
//
// 1 setdrawmode                           # set 'direct' mode
//
void gfx_prim_setdrawmode()
{
  arg_t *argv = gfx_arg_1(OTYPE_NUM);

  if(!argv) return;

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) canvas->draw_mode = OBJ_VALUE_FROM_PTR(argv[0].ptr);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get drawing region
//
// group: gfx
//
// ( -- int_1 int_2 int_3 int_4 )
// int_1: x
// int_2: y
// int_3: width
// int_4: height
//
// Get drawing region associated with current graphics state. Any drawing operation
// will be relative to this region. Graphics output will be clipped at the
// region boundaries.
//
// example:
//
// getregion                  # 0 0 800 600
//
void gfx_prim_getregion()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  int64_t x = 0, y = 0, width = 0, height = 0;

  if(canvas) {
    x = canvas->region.x;
    y = canvas->region.y;
    width = canvas->region.width;
    height = canvas->region.height;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(x, t_int), 0);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(y, t_int), 0);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(width, t_int), 0);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(height, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set drawing region
//
// group: gfx
//
// ( int_1 int_2 int_3 int_4 -- )
// int_1: x
// int_2: y
// int_3: width
// int_4: height
//
// Set drawing region associated with current graphics state. Any drawing operation
// will be relative to this region. Graphics output will be clipped at the
// region boundaries.
//
// example:
//
// 10 10 200 100 setregion
//
void gfx_prim_setregion()
{
  arg_t *argv = gfx_arg_n(4, (uint8_t [4]) { OTYPE_NUM, OTYPE_NUM, OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val0 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[1].ptr);
  int64_t val2 = OBJ_VALUE_FROM_PTR(argv[2].ptr);
  int64_t val3 = OBJ_VALUE_FROM_PTR(argv[3].ptr);

  area_t area = { .x = val0, .y = val1, .width = val2, .height = val3 };

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) canvas->region = area;

  gfx_obj_array_pop_n(4, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get location
//
// group: gfx
//
// ( -- int_1 int_2 )
// int_1: x
// int_2: y
//
// Get location associated with current graphics state.
//
// example:
//
// getlocation                  # 0 0
//
void gfx_prim_getlocation()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  int64_t x = 0, y = 0;

  if(canvas) {
    x = canvas->geo.x;
    y = canvas->geo.y;
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(x, t_int), 0);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(y, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set location
//
// group: gfx
//
// ( int_1 int_2 -- )
// int_1: x
// int_2: y
//
// Set location associated with current graphics state.
//
// example:
//
// 10 10 setlocation
//
void gfx_prim_setlocation()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val0 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[1].ptr);

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    canvas->geo.x = val0;
    canvas->geo.y = val1;
  }

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get default canvas
//
// group: gfx
//
// ( -- canvas_1 )
// ( -- nil )
//
// Get default canvas used for graphics operations. If none has been set, return nil.
//
// A canvas has associated
//   - a size and position on screen (see 'getlocation')
//   - a rectangular region used for drawing and clipping (see 'getregion')
//   - a cursor position (see 'getpos')
//   - a font (see 'getfont')
//   - a color (see 'getcolor')
//   - a background color - used in debug console (see 'getbgcolor')
//   - a drawing mode (see 'getdrawmode')
//
// example:
//
// # get current default canvas
//
// /current_canvas getcanvas def
//
void gfx_prim_getcanvas()
{
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfxboot_data->canvas_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set default canvas
//
// group: gfx
//
// ( canvas_1 -- )
// ( nil -- )
//
// Set default canvas. If nil is passed, there will be no default canvas.
//
// example:
//
// /saved_state getcanvas def                   # save current graphics state
// ...
// saved_state setcanvas                        # restore saved graphics state
//
void gfx_prim_setcanvas()
{
  arg_t *argv = gfx_arg_1(OTYPE_CANVAS | IS_NIL);

  if(!argv) return;

  OBJ_ID_ASSIGN(gfxboot_data->canvas_id, argv[0].id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// create canvas
//
// group: gfx
//
// ( int_1 int_2 -- canvas_1 )
// int_1: width
// int_2: height
//
// Create a new empty canvas of the specified size.
//
// example:
//
// 800 600 newcanvas
//
void gfx_prim_newcanvas()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val0 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[1].ptr);

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_canvas_new(val0, val1), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get debug console canvas
//
// group: gfx
//
// ( -- canvas_1 )
// ( -- nil )
//
// Get canvas of the debug console. If none has been set, return nil.
//
// example:
//
// # get console font
//
// /console_font getconsole getfont def
//
void gfx_prim_getconsole()
{
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfxboot_data->console.canvas_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set debug console canvas
//
// group: gfx
//
// ( canvas_1 -- )
// ( nil -- )
//
// Set canvas of the debug console. If nil is passed, the current canvas is removed
// (and debug console disabled).
//
// You can use this to change the appearance of the debug console.
//
// example:
//
// # change debug console backgound color to transparent light blue
//
// getcanvas getconsole setcanvas 0x40405070 setbgcolor setcanvas
//
void gfx_prim_setconsole()
{
  arg_t *argv = gfx_arg_1(OTYPE_CANVAS | IS_NIL);

  if(!argv) return;

  OBJ_ID_ASSIGN(gfxboot_data->console.canvas_id, argv[0].id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// show
//
// group: gfx
//
// ( string_1 -- )
// ( int_1 -- )
//
// Print string string_1 or character int_1 at current cursor position in default canvas.
//
// The cursor position is advanced to point at the end of the printed text.
// In strings, newline ('\x0a') and carriage return ('\x0d') characters are interpreted
// and the cursor position is adjusted relative to the starting position.
//
// example:
//
// "Hello!" show                        # print "Hello!"
// 65 show                              # print "A"
//
void gfx_prim_show()
{
  arg_t *argv = gfx_arg_1(OTYPE_ANY);

  if(!argv) return;

  switch(argv[0].ptr->base_type) {
    case OTYPE_NUM:
      int64_t chr = OBJ_VALUE_FROM_PTR(argv[0].ptr);
      if(chr < 0) chr = 0xfffd;
      gfx_putc(gfxboot_data->canvas_id, chr, 1);
      break;

    case OTYPE_MEM:
      data_t *data = OBJ_DATA_FROM_PTR(argv[0].ptr);
      gfx_puts(gfxboot_data->canvas_id, data->ptr, data->size);
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get graphics object dimension
//
// group: gfx
//
// ( canvas_1 -- int_1 int_2 )
// ( font_1 -- int_1 int_2 )
// int_1: width
// int_2: height
//
// Get dimension of graphics object. For a canvas it is its size, for a fixed size
// font it is its glyph size, for a proportional font the width is 0 and the
// height is the font height.
//
// example:
//
// getconsole dim                         # 640 480
// getconsole setcanvas getfont dim       # 8 16
//
void gfx_prim_dim()
{
  arg_t *argv = gfx_arg_1(OTYPE_ANY);

  if(!argv) return;

  area_t area = {};

  switch(argv[0].ptr->base_type) {
    case OTYPE_NUM:
      int64_t chr = OBJ_VALUE_FROM_PTR(argv[0].ptr);
      if(chr < 0) chr = 0xfffd;
      area = gfx_char_dim(gfxboot_data->canvas_id, chr);
      break;

    case OTYPE_FONT:
      area = gfx_font_dim(argv[0].id);
      break;

    case OTYPE_MEM:
      data_t *data = OBJ_DATA_FROM_PTR(argv[0].ptr);
      area = gfx_text_dim(gfxboot_data->canvas_id, data->ptr, data->size);
      break;

    case OTYPE_CANVAS:
      canvas_t *canvas = OBJ_CANVAS_FROM_PTR(argv[0].ptr);
      area.width = canvas->geo.width;
      area.height = canvas->geo.height;
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(area.width, t_int), 0);
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfx_obj_num_new(area.height, t_int), 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// run code
//
// group: loop
//
// ( string_1 -- )
// string_1: binary code
//
// Load binary code and run it.
//
// Note: unlike 'exec' this does not open a new context but replaces the
// currently running code with the new one.
//
// example:
//
// "new_program" readfile run
//
void gfx_prim_run()
{
  arg_t *argv = gfx_arg_1(OTYPE_MEM);

  if(!argv) return;

  if(!gfx_is_code(argv[0].id)) {
    GFX_ERROR(err_invalid_code);

    return;
  }

  context_t *context = gfx_obj_context_ptr(gfxboot_data->vm.program.context);

  if(!context) {
    GFX_ERROR(err_internal);

    return;
  }

  OBJ_ID_ASSIGN(context->code_id, argv[0].id);

  context->ip = context->current_ip = 0;

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// read file
//
// group: mem
//
// ( string_1 -- string_2 )
// ( string_1 -- nil )
// string_1: file name
// string_2: file content
// 
// Read entire file and return its content. If the file could not be read, return nil.
//
// example:
//
// "foo" readfile
//
void gfx_prim_readfile()
{
  arg_t *argv = gfx_arg_1(OTYPE_MEM);

  if(!argv) return;

  // interface expects 0-terminated string
  // so we create a 1 byte larger copy
  obj_id_t tmp_id = gfx_obj_mem_dup(argv[0].id, 1);
  data_t *data = gfx_obj_mem_ptr(tmp_id);

  if(!data) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  obj_id_t file_id = gfx_read_file(data->ptr);

  gfx_obj_ref_dec(tmp_id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, file_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// unpack image
//
// group: gfx
//
// ( string_1 -- canvas_1 )
// ( string_1 -- nil )
// string_1: image file data
//
// Unpacks image and returns a canvas object with the image or nil if the
// data does not contain image data.
//
// example:
//
// "foo.jpg" readfile unpackimage
//
void gfx_prim_unpackimage()
{
  arg_t *argv = gfx_arg_1(OTYPE_MEM);

  if(!argv) return;

  obj_id_t image_id = gfx_image_open(argv[0].id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, image_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// copy rectangular region
//
// group: gfx
//
// ( canvas_1 canvas_2 -- )
//
// Copy content from drawing pos in drawing region of canvas_2 to the
// drawing pos in drawing region of canvas_1, using drawing mode of
// canvas_1.
//
// example:
//
// # show cat picture
//
// /cat_pic "cat.jpg" readfile unpackimage def
// 300 200 setpos getcanvas cat_pic blt
//
void gfx_prim_blt()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_CANVAS, OTYPE_CANVAS });

  if(!argv) return;

  canvas_t *canvas1 = OBJ_CANVAS_FROM_PTR(argv[0].ptr);
  canvas_t *canvas2 = OBJ_CANVAS_FROM_PTR(argv[1].ptr);

  area_t area2 = canvas2->region;
  area2.x += canvas2->cursor.x;
  area2.y += canvas2->cursor.y;

  area_t diff2 = gfx_clip(&area2, &canvas2->region);

#if 0
  gfxboot_serial(0, "blt clipped area2 %dx%d_%dx%d\n",
    area2.x, area2.y, area2.width, area2.height
  );
  gfxboot_serial(0, "blt diff2 %dx%d_%dx%d\n",
    diff2.x, diff2.y, diff2.width, diff2.height
  );
#endif

  area_t area1 = {
    .x = canvas1->region.x + canvas1->cursor.x + diff2.x,
    .y = canvas1->region.y + canvas1->cursor.y + diff2.y,
    .width = area2.width,
    .height = area2.height
  };

#if 0
  gfxboot_serial(4, "blt area1 %dx%d_%dx%d, area2 %dx%d_%dx%d\n",
    area1.x, area1.y, area1.width, area1.height,
    area2.x, area2.y, area2.width, area2.height
  );
#endif

  area_t diff = gfx_clip(&area1, &canvas1->region);

  ADD_AREA(area2, diff);

#if 0
  gfxboot_serial(0, "blt dst #%d (%dx%d_%dx%d), src #%d (%dx%d_%dx%d)\n",
    OBJ_ID2IDX(argv[0].id),
    area1.x, area1.y, area1.width, area1.height,
    OBJ_ID2IDX(argv[1].id),
    area2.x, area2.y, area2.width, area2.height
  );
#endif

  gfx_blt(canvas1->draw_mode, argv[0].id, area1, argv[1].id, area2);

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// start debug console
//
// group: def
//
// ( -- )
//
// Stop code execution and start debug console.
//
// You can leave (and re-enter) the debug console with `^D` but note that this
// doesn't resume program execution. Use the `run` (or `r`) console command for this.
//
// example:
//
// /foo { debug 10 20 } def
// foo                                  # activate debug console when 'foo' is run
//
void gfx_prim_debug()
{
  gfxboot_data->vm.program.stop = 1;
  gfx_program_debug_on_off(1, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// run debug command
//
// group: def
//
// ( str_1 -- )
// ( bool_1 -- )
//
// Show debug console and run debug command.
//
// If a string is passed, turn on debug console and run debug command.
//
// If true is passed, turn on debug console (without capturing input).
// If false is passed, turn off debug console.
//
// example:
//
// /foo { 10 20 "p stack" debugcmd } def
// foo                                  # activate debug console and show current stack
//
void gfx_prim_debugcmd()
{
  arg_t *argv = gfx_arg_1(OTYPE_ANY);

  if(!argv) return;

  int pstack_freed = 0;

  switch(argv[0].ptr->base_type) {
    case OTYPE_NUM:
      int64_t val = OBJ_VALUE_FROM_PTR(argv[0].ptr);
      if(val) {
        gfx_program_debug_on_off(1, 0);
      }
      else {
        gfx_program_debug_on_off(0, 0);
      }
      break;

    case OTYPE_MEM:
      data_t *data = OBJ_DATA_FROM_PTR(argv[0].ptr);
      char buf[256];
      unsigned buf_len = data->size;
      if(buf_len > sizeof buf - 1) buf_len = sizeof buf - 1;
      gfx_memcpy(buf, data->ptr, buf_len);
      buf[buf_len] = 0;
      gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
      pstack_freed = 1;
      gfx_program_debug_on_off(1, 0);
      gfx_debug_cmd(buf);
      break;

    default:
      GFX_ERROR(err_invalid_arguments);
      return;
  }

  if(!pstack_freed) gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// read pixel
//
// group: gfx 
//
// ( -- int_1 )
// ( -- nil )
// int_1: color
//
// Read pixel at drawing position from canvas in current graphics state. If
// the position is outside the drawing region, return nil.
//
// example:
//
// getpixel
//
void gfx_prim_getpixel()
{
  obj_id_t val = 0;

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    color_t color;
    if(gfx_getpixel(gfxboot_data->canvas_id, canvas->cursor.x, canvas->cursor.y, &color)) {
      val = gfx_obj_num_new(color, t_int);
    }
  }

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, val, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set pixel
//
// group: gfx 
//
// ( -- )
//
// Set pixel with current color at drawing position in canvas in current
// graphics state. If the position is outside the drawing region, nothing is
// drawn.
//
// example:
//
// setpixel
//
void gfx_prim_putpixel()
{
  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    gfx_putpixel(gfxboot_data->canvas_id, canvas->cursor.x, canvas->cursor.y, canvas->color);
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// draw line
//
//group: gfx
//
// ( int_1 int_2 -- )
// int_1: x
// int_2: y
//
// Draw line from current position to the specified x and y coordinates
// using the current color. The drawing position is updated to the end
// position. Line segments outside the drawing region are not drawn.
//
// example:
//
// 100 200 drawline
//
void gfx_prim_drawline()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val2 = OBJ_VALUE_FROM_PTR(argv[1].ptr);

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    gfx_line(gfxboot_data->canvas_id, canvas->cursor.x, canvas->cursor.y, val1, val2, canvas->color);

    canvas->cursor.x = val1;
    canvas->cursor.y = val2;
  }

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// draw filled rectangle
//
// group: gfx
//
// ( int_1 int_2 -- )
// int_1: width
// int_2: height
//
// Draw filled rectangle (using current color) at current position. The
// rectangle is clipped at the current drawing region.
//
// example:
//
// 200 100 fillrect
//
void gfx_prim_fillrect()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val2 = OBJ_VALUE_FROM_PTR(argv[1].ptr);

  canvas_t *canvas = gfx_obj_canvas_ptr(gfxboot_data->canvas_id);

  if(canvas) {
    gfx_rect(gfxboot_data->canvas_id, canvas->cursor.x, canvas->cursor.y, val1, val2, canvas->color);
  }

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// decode Unicode string
//
// group: mem
//
// ( string_1 -- array_1 )
// string_1: UTF8-encoded string
// array_1: array with decoded chars
//
// The array contains one element for each UTF8-encoded char. If string_1
// contains non-UTF8-chars they are represented as the negated 8-bit value.
//
// example:
//
// "ABC" decodeutf8                     # [ 65 66 67 ]
// "Ä €" decodeutf8                     # [ 196 32 8364 ]
// "A\xf0B" decodeutf8                  # [ 65 -240 66 ]
//
void gfx_prim_decodeutf8()
{
  arg_t *argv = gfx_arg_1(OTYPE_MEM);

  if(!argv) return;

  data_t *mem = OBJ_DATA_FROM_PTR(argv[0].ptr);

  char *data = (char *) mem->ptr;
  unsigned data_len = mem->size;

  unsigned uni_len = 0;
  while(data_len) {
    uni_len++;
    gfx_utf8_dec(&data, &data_len);
  }

  obj_id_t array_id = gfx_obj_array_new(uni_len);
  if(!array_id) {
    GFX_ERROR(err_no_memory);
    return;
  }

  data = (char *) mem->ptr;
  data_len = mem->size;

  int i = 0;
  while(data_len) {
    int c = gfx_utf8_dec(&data, &data_len);
    gfx_obj_array_set(array_id, gfx_obj_num_new(c, t_int), i++, 0);
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, array_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// encode Unicode string
//
// group: mem
//
// ( array_1 -- string_1 )
// array_1: array with decoded chars
// string_1: UTF8-encoded string
//
// The array contains one element for each UTF8-encoded char. If string_1
// should contain non-UTF8-chars they are represented as the negated 8-bit
// value in array_1.
//
// example:
//
// [ 65 66 67 ] encodeutf8              # "ABC"
// [ 196 32 8364 ] encodeutf8           # "Ä €"
// [ 65 -240 66 ] encodeutf8            # "A\xf0B"
//
void gfx_prim_encodeutf8()
{
  arg_t *argv = gfx_arg_1(OTYPE_ARRAY);

  if(!argv) return;

  obj_id_t array_id = argv[0].id;
  unsigned array_size = OBJ_ARRAY_FROM_PTR(argv[0].ptr)->size;

  obj_id_t mem_id = gfx_obj_mem_new(array_size * 6, t_string);
  if(!mem_id) {
    GFX_ERROR(err_no_memory);
    return;
  }

  obj_t *ptr = gfx_obj_ptr(mem_id);
  data_t *mem = OBJ_DATA_FROM_PTR(ptr);

  uint8_t *data = (uint8_t *) mem->ptr;

  for(int i = 0; i < (int) array_size; i++) {
    int64_t *val = gfx_obj_num_ptr(gfx_obj_array_get(array_id, i));

    if(!val) {
      gfx_obj_ref_dec(mem_id);
      GFX_ERROR(err_invalid_data);
      return;
    }

    if(*val <= 0) {
      *data++ = -*val;
    }
    else {
      uint8_t *str = (uint8_t *) gfx_utf8_enc(*val);
      while(*str) *data++ = *str++;
    }
  }

  gfx_obj_realloc(mem_id, data - (uint8_t *) mem->ptr);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, mem_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// format string
//
// group: mem
//
// ( string_1 array_1 -- string_2 )
// string_1: printf-style format string
// array_1: array with to-be-formatted arguments
// string_2: formatted string
//
// example:
//
// "int = %d" [ 200 ] format            # "int = 200"
// "string = %s" [ "foo" ] format       # "string = foo"
// "%s: %d" [ "bar" 33 ] format         # "bar: 33"
//
void gfx_prim_format()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_MEM, OTYPE_ARRAY });

  if(!argv) return;

  data_t *format_data = gfx_obj_mem_ptr(argv[0].id);
  uint8_t *format_str = format_data->ptr;
  unsigned format_size = format_data->size;

  obj_id_t result_id = gfx_obj_mem_new(0, t_string);

  int arg_pos = 0;

  struct format_spec_s {
    unsigned active:1;
    unsigned zero:1;
    unsigned with_precision:1;
    unsigned left:1;
    int width;
    int precision;
    uint8_t type;
    uint8_t sign;
  } format_spec = { };

  int out_pos = 0;

  for(unsigned f_pos = 0; f_pos < format_size; f_pos++) {
    uint8_t f_val = format_str[f_pos];
    if(format_spec.active) {
      if(f_val == '%') {
        format_spec.active = 0;
        gfx_obj_mem_set(result_id, f_val, out_pos++);
      }
      else if(f_val == ' ') {
        format_spec.sign = ' ';
        continue;
      }
      else if(f_val == '-') {
        format_spec.left = 1;
        continue;
      }
      else if(f_val == '+') {
        format_spec.sign = '+';
        continue;
      }
      else if(f_val == '.') {
        format_spec.with_precision = 1;
        continue;
      }
      else if(f_val >= '0' && f_val <= '9') {
        if(format_spec.with_precision) {
          format_spec.precision = format_spec.precision * 10 + f_val - '0';
        }
        else {
          if(f_val == '0' && !format_spec.width) {
            format_spec.zero = 1;
          }
          else {
            format_spec.width = format_spec.width * 10 + f_val - '0';
          }
        }
        continue;
      }
      else if(f_val == 'd' || f_val == 'u' || f_val == 'x' || f_val == 's') {
        uint8_t *data_ptr;
        int len;
        if(f_val == 's') {
          data_t *str = gfx_obj_mem_ptr(gfx_obj_array_get(argv[1].id, arg_pos));
          if(str) {
            data_ptr = str->ptr;
            len = (int) str->size;
          }
          else {
            data_ptr = "nil";
            len = gfx_strlen(data_ptr);
          }

          if(format_spec.precision && format_spec.precision < len) len = format_spec.precision;
        }
        else {
          uint8_t buf[32];	// large enough for 64 bit numbers
          uint64_t *num = gfx_obj_num_ptr(gfx_obj_array_get(argv[1].id, arg_pos));

          if(format_spec.precision) {
            if(num) format_spec.zero = 1;	// not for nil
            if(format_spec.precision > format_spec.width) format_spec.width = format_spec.precision;
          }

          if(num) {
            char f[8] = { '%' };
            char *fp = f + 1;
            if(format_spec.sign) *fp++ = (char) format_spec.sign;
            *fp++ = 'l';
            *fp++ = 'l';
            *fp++ = (char) f_val;
            gfxboot_snprintf(buf, sizeof buf, f, (long long) *num);
            data_ptr = buf;
          }
          else {
            data_ptr = "nil";
          }
          len = gfx_strlen(data_ptr);
        }

        int full_len = format_spec.width > len ? format_spec.width : len;
        int len_diff = full_len - len;
        if(!format_spec.left) {
          while(len_diff) {
            gfx_obj_mem_set(result_id, format_spec.zero ? '0' : ' ', out_pos++);
            len_diff--;
          }
        }
        for(int i = 0; i < len; i++) {
          gfx_obj_mem_set(result_id, data_ptr[i], out_pos++);
        }
        if(format_spec.left) {
          while(len_diff) {
            gfx_obj_mem_set(result_id, format_spec.zero ? '0' : ' ', out_pos++);
            len_diff--;
          }
        }
      }
      else {
        // unhandled format
      }
      format_spec.active = 0;
      arg_pos++;
    }
    else {
      if(f_val == '%') {
        format_spec = (struct format_spec_s) { active: 1 };
      }
      else {
        gfx_obj_mem_set(result_id, f_val, out_pos++);
      }
    }
  }

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, result_id, 0);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get compose list
//
// group: gfx
//
// ( -- array_1 )
// ( -- nil )
//
// Get current compose list. If none has been set, return nil.
//
// The compose list is an array of graphics states.
//
// example:
//
// /current_list getcompose def                   # get current list of visible graphics states
//
void gfx_prim_getcompose()
{
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfxboot_data->compose.list_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set compose list
//
// group: gfx
//
// ( array_1 -- )
// ( nil -- )
//
// Set current compose list. If nil is passed, the current list is removed.
//
// The compose list is an array of canvas objects.
//
// example:
//
// /saved_list getcompose def                   # save current list
// ...
// saved_list setcompose                        # restore list
//
void gfx_prim_setcompose()
{
  arg_t *argv = gfx_arg_1(OTYPE_ARRAY | IS_NIL);

  if(!argv) return;

  OBJ_ID_ASSIGN(gfxboot_data->compose.list_id, argv[0].id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// update screen region
//
// group: gfx
//
// ( int_1 int_2 int_3 int_4 -- )
// int_1: x
// int_2: y
// int_3: width
// int_4: height
//
// Update (redraw) screen region.
//
// example:
//
// 10 10 200 100 updatescreen
//
void gfx_prim_updatescreen()
{
  arg_t *argv = gfx_arg_n(4, (uint8_t [4]) { OTYPE_NUM, OTYPE_NUM, OTYPE_NUM, OTYPE_NUM });

  if(!argv) return;

  int64_t val1 = OBJ_VALUE_FROM_PTR(argv[0].ptr);
  int64_t val2 = OBJ_VALUE_FROM_PTR(argv[1].ptr);
  int64_t val3 = OBJ_VALUE_FROM_PTR(argv[2].ptr);
  int64_t val4 = OBJ_VALUE_FROM_PTR(argv[3].ptr);

  area_t area = { .x = val1, .y = val2, .width = val3, .height = val4 };

  gfx_screen_compose(area);

  gfx_obj_array_pop_n(4, gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get event handler
//
// group: system
//
// ( -- code_1 )
// ( -- nil )
//
// Get current event handler. If none has been set, return nil.
//
// The event handler is a reference to a code blöck or function.
//
// example:
//
// /current_handler geteventhandler def                   # get current event handler
//
void gfx_prim_geteventhandler()
{
  gfx_obj_array_push(gfxboot_data->vm.program.pstack, gfxboot_data->event_handler_id, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// set event handler
//
// group: system
//
// ( code_1 -- )
// ( nil -- )
//
// Set current event handler. If nil is passed, the current event handler is removed.
//
// The event handler is a reference to a code blöck or function.
//
// example:
//
// /old_handler geteventhandler def             # save current event handler
// ...
// old_handler seteventhandler                  # restore event handler
//
void gfx_prim_seteventhandler()
{
  arg_t *argv = gfx_arg_1(OTYPE_MEM | IS_NIL);

  if(!argv) return;

  obj_id_t id = argv[0].id;
  obj_t *ptr = argv[0].ptr;

  if(ptr && ptr->sub_type == t_ref) {
    id = gfx_lookup_dict(OBJ_DATA_FROM_PTR(ptr)).id2;
    ptr = gfx_obj_ptr(id);
  }

  if(
    ptr && ptr->sub_type != t_code
  ) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  OBJ_ID_ASSIGN(gfxboot_data->event_handler_id, id);

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// turn hash into class
//
// group: def
//
// ( ref_1 hash_1 hash_2 -- ref_1 hash_1 )
// ref_1: the class name
// hash_1: hash to be made a class
// hash_2: parent class or nil
//
// Turn regular hash into a class object.
//
// example:
//
// /Foo (
//   /init { }
//   /foo_method  { "foo" }
// ) nil class def
//
// Define class Bar, derived from Foo:
//
// /Bar (
//   /bar_method { "bar" }
// ) Foo class def
//
void gfx_prim_class()
{
  arg_t *argv = gfx_arg_n(3, (uint8_t [3]) { OTYPE_MEM, OTYPE_HASH, OTYPE_HASH | IS_NIL });

  if(!argv) return;

  if(
    (argv[0].ptr->sub_type != t_ref) ||
    (argv[2].ptr && !argv[2].ptr->flags.hash_is_class)
  ) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  arg_t class_name = { .id = gfx_obj_mem_dup(argv[0].id, 0) };

  OBJ_PTR_UPDATE(class_name);

  if(class_name.ptr) {
    class_name.ptr->sub_type = t_string;
    class_name.ptr->flags.ro = 1;
  }

  gfx_obj_hash_set(argv[1].id, gfx_obj_asciiz_new("class"), class_name.id, 0);

  OBJ_PTR_UPDATE(argv[1]);

  if(argv[1].ptr) {
    argv[1].ptr->flags.ro = 1;
    argv[1].ptr->flags.hash_is_class = 1;

    OBJ_ID_ASSIGN(OBJ_HASH_FROM_PTR(argv[1].ptr)->parent_id, argv[2].id);
  }

  gfx_obj_array_pop(gfxboot_data->vm.program.pstack, 1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// create class instance
//
// group: def
//
// ( hash_1 hash_2 -- hash_3 )
// hash_1: the class
// hash_2: hash with arguments for init method
// hash_3: initialized class instance
//
// Initialize a new class instance.
// 'init' method (if it exists) will be called implicitly.
//
// example:
//
// /Foo (
//   /x 0
//   /init { }
//   /bar  { x }
// ) nil class def
//
// /foo Foo ( /x 100 ) new def
//
// foo .bar
//
void gfx_prim_new()
{
  arg_t *argv = gfx_arg_n(2, (uint8_t [2]) { OTYPE_HASH, OTYPE_HASH });

  if(!argv) return;

  if(!argv[0].ptr->flags.hash_is_class) {
    GFX_ERROR(err_invalid_arguments);
    return;
  }

  hash_t *hash_vars = OBJ_HASH_FROM_PTR(argv[1].ptr);

  arg_t dict = { .id = gfx_obj_hash_new(hash_vars->size) };

  // copy hash with vars to new dict
  obj_id_t key, val;
  unsigned idx = 0;
  while(gfx_obj_iterate(argv[1].id, &idx, &key, &val)) {
    // note: reference counting for key & val has been done inside gfx_obj_iterate()
    gfx_obj_hash_set(dict.id, key, val, 0);
  }

  OBJ_PTR_UPDATE(dict);

  if(dict.ptr) {
    dict.ptr->flags.sticky = 1;
    dict.ptr->flags.hash_is_class = 1;
    OBJ_ID_ASSIGN(OBJ_HASH_FROM_PTR(dict.ptr)->parent_id, argv[0].id);
  }

  gfx_obj_array_pop_n(2, gfxboot_data->vm.program.pstack, 1);

  gfx_obj_array_push(gfxboot_data->vm.program.pstack, dict.id, 0);

  // now run init method
  obj_id_pair_t pair = gfx_obj_hash_get(dict.id, & (data_t) { .ptr = "init", .size = sizeof "init" - 1 });

  if(pair.id1) {
    gfx_exec_id(dict.id, pair.id2, 0);
  }
}
