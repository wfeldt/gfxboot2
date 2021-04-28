#include <gfxboot/gfxboot.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// hash

static unsigned find_key(hash_t *hash, data_t *key, int *match);


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_hash_new(unsigned max)
{
  if(!max) max = 0x10;

  obj_id_t id = gfx_obj_alloc(OTYPE_HASH, OBJ_HASH_SIZE(max));
  hash_t *h = gfx_obj_hash_ptr(id);

  if(h) {
    h->max = max;
  }

  return id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
hash_t *gfx_obj_hash_ptr(obj_id_t id)
{
  obj_t *ptr = gfx_obj_ptr(id);

  if(!ptr || ptr->base_type != OTYPE_HASH) return 0;

  return (hash_t *) ptr->data.ptr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_hash_iterate(obj_t *ptr, unsigned *idx, obj_id_t *id1, obj_id_t *id2)
{
  hash_t *h = ptr->data.ptr;

  if(ptr->data.size != OBJ_HASH_SIZE(h->max)) {
    GFX_ERROR(err_internal);
    return *idx = 0;
  }

  if(*idx >= h->size) {
    return 0;
  }

  gfx_obj_ref_inc(*id1 = h->ptr[*idx].key);
  gfx_obj_ref_inc(*id2 = h->ptr[*idx].value);
  (*idx)++;

  return 2;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_hash_dump(obj_t *ptr, dump_style_t style)
{
  if(!ptr) return 1;

  hash_t *h = ptr->data.ptr;
  unsigned u;
  obj_id_t key, value;

  if(ptr->data.size != OBJ_HASH_SIZE(h->max)) {
    if(style.ref) {
      gfxboot_log("      <invalid data>\n");
    }
    else {
      gfxboot_log("<invalid data>");
    }

    return 1;
  }

  if(!style.ref) {
    if(!style.inspect) return 0;

    gfxboot_log("size %u, max %u", h->size, h->max);
    if(h->parent_id) gfxboot_log(", parent %s", gfx_obj_id2str(h->parent_id));

    return 1;
  }

  for(u = 0; u < h->size && (!style.max || u < style.max); u++) {
    key = h->ptr[u].key;
    if(!key) continue;
    value = h->ptr[u].value;
    if(style.dump) gfxboot_log("    ");
    gfx_obj_dump(key, (dump_style_t) { .inspect = style.inspect, .no_nl = 1 });
    gfxboot_log(" => ");
    gfx_obj_dump(value, (dump_style_t) { .inspect = style.inspect, .no_nl = 1 });
    gfxboot_log("\n");
  }

  return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_t gfx_obj_hash_set(obj_id_t hash_id, obj_id_t key_id, obj_id_t value_id, int do_ref_cnt)
{
  unsigned u;
  int match;

  hash_t *hash = gfx_obj_hash_ptr(hash_id);
  if(!hash) return 0;

  data_t *key = gfx_obj_mem_ptr(key_id);
  if(!key) return 0;

  u = find_key(hash, key, &match);
  // gfxboot_log("XXX set: key %s, u %d, match %d\n", (char *) key->ptr, (int) u, match);

  if(!match) {
    hash->size++;
    if(hash->size > hash->max) {
      unsigned max = hash->max + (hash->max >> 3) + 0x10;
      hash_id = gfx_obj_realloc(hash_id, OBJ_HASH_SIZE(max));
      if(!hash_id) return 0;
      hash = gfx_obj_hash_ptr(hash_id);
      if(!hash) return 0;
      hash->max = max;
    }
    if(u + 1 < hash->size) {
      // gfxboot_log("XXX set: move %d -> %d [%d]\n", (int) u, (int) u + 1, (int) (hash->size - u - 1));
      gfx_memcpy(hash->ptr + u + 1, hash->ptr + u, (sizeof *hash->ptr) * (hash->size - u - 1));
      hash->ptr[u].key = 0;
      hash->ptr[u].value = 0;
    }
  }

  obj_id_t orig_key_id = 0;

  // if key is writable string, make a copy
  obj_t *key_obj = gfx_obj_ptr(key_id);
  if(key_obj && !key_obj->flags.ro) {
    orig_key_id = key_id;
    key_id = gfx_obj_mem_dup(key_id, 0);
  }

  if(do_ref_cnt) {
    if(!orig_key_id) gfx_obj_ref_inc(key_id);
    gfx_obj_ref_inc(value_id);
    gfx_obj_ref_dec(hash->ptr[u].key);
    gfx_obj_ref_dec(hash->ptr[u].value);
  }

  // this is a bit tricky: release orig_key_id regardless of do_ref_cnt
  if(orig_key_id) gfx_obj_ref_dec(orig_key_id);

  hash->ptr[u].key = key_id;
  hash->ptr[u].value = value_id;

  return hash_id;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
obj_id_pair_t gfx_obj_hash_get(obj_id_t hash_id, data_t *key)
{
  obj_id_t orig_hash_id = hash_id;
  unsigned u, level = 0;
  int match;
  hash_t *hash;

  do {
    // Return hash_id for hash where search started - or hash where key was found in?
    // With next line it's the second. Remove that line for first variant.
    orig_hash_id = hash_id;

    hash = gfx_obj_hash_ptr(hash_id);

    if(!hash) return (obj_id_pair_t) {};

    u = find_key(hash, key, &match);
    // gfxboot_log("XXX get key %s, u %d, match %d\n", (char *) key->ptr, (int) u, match);

    hash_id = hash->parent_id;
  }
  while(!match && hash_id && level++ < 100);

  if(!match) return (obj_id_pair_t) {};

  return (obj_id_pair_t) { .id1 = orig_hash_id, .id2 = hash->ptr[u].value };
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void gfx_obj_hash_del(obj_id_t hash_id, obj_id_t key_id, int do_ref_cnt)
{
  unsigned u;
  int match;

  hash_t *hash = gfx_obj_hash_ptr(hash_id);
  if(!hash) return;

  data_t *key = gfx_obj_mem_ptr(key_id);
  if(!key) return;

  u = find_key(hash, key, &match);
  // gfxboot_log("XXX del key %s, u %d, match %d\n", (char *) key->ptr, (int) u, match);

  if(match) {
    if(do_ref_cnt) {
      gfx_obj_ref_dec(hash->ptr[u].key);
      gfx_obj_ref_dec(hash->ptr[u].value);
    }

    hash->size--;

    if(u < hash->size) {
      // gfxboot_log("XXX del: move %d -> %d [%d]\n", (int) u + 1, (int) u, (int) (hash->size - u));
      gfx_memcpy(hash->ptr + u, hash->ptr + u + 1, (sizeof *hash->ptr) * (hash->size - u));
    }

    hash->ptr[hash->size].key = 0;
    hash->ptr[hash->size].value = 0;
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
unsigned gfx_obj_hash_gc(obj_t *ptr)
{
  if(!ptr) return 0;

  hash_t *hash = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned idx, more_gc = 0;

  if(hash && data_size == OBJ_HASH_SIZE(hash->max)) {
    more_gc += gfx_obj_ref_dec_delay_gc(hash->parent_id);
    for(idx = 0; idx < hash->size; idx++) {
      more_gc += gfx_obj_ref_dec_delay_gc(hash->ptr[idx].key);
      more_gc += gfx_obj_ref_dec_delay_gc(hash->ptr[idx].value);
    }
  }

  return more_gc;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int gfx_obj_hash_contains(obj_t *ptr, obj_id_t id)
{
  if(!ptr || !id) return 0;

  hash_t *hash = ptr->data.ptr;
  unsigned data_size = ptr->data.size;
  unsigned idx;

  if(hash && data_size == OBJ_HASH_SIZE(hash->max)) {
    if(hash->parent_id == id) return 1;
    for(idx = 0; idx < hash->size; idx++) {
      if(
        id == hash->ptr[idx].key ||
        id == hash->ptr[idx].value
      ) return 1;
    }
  }

  return 0;
}

#if 0

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// linear search
unsigned find_key(hash_t *hash, data_t *key, int *match)
{
  unsigned u;
  data_t *hash_key;

  *match = 0;

  for(u = 0; u < hash->size; u++) {
    hash_key = gfx_obj_mem_ptr(hash->ptr[u].key);
    if(!hash_key) continue;
    int i = gfx_obj_mem_cmp(key, hash_key);
    if(i > 0) continue;
    if(i == 0) *match = 1;
    break;
  }

  return u;
}

#else

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// binary search
unsigned find_key(hash_t *hash, data_t *key, int *match)
{
  unsigned u_start, u_end, u;
  data_t *hash_key;

  *match = 0;

  u_start = u = 0;
  u_end = hash->size;

  while(u_end > u_start) {
    u = (u_end + u_start) / 2;

    hash_key = gfx_obj_mem_ptr(hash->ptr[u].key);
    if(!hash_key) return 0;

    int i = gfx_obj_mem_cmp(key, hash_key);

    if(i == 0) {
      *match = 1;
      break;
    }

    if(u_end == u_start + 1) {
      if(i > 0) u++;
      break;
    }

    if(i > 0) {
      if(u_end == u + 1) {
        if(i > 0) u++;
        break;
      }
      u_start = u;
    }
    else {
      u_end = u;
    }
  }

  return u;
}
#endif
