#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <ctype.h>

// initial vocabulary (note: "{" & "}" are special)
#define WITH_PRIM_NAMES 1
#define WITH_TYPE_NAMES 1
#include "vocabulary.h"

#define COMMENT_CHAR	'#'

#define MAX_INCLUDE	16

typedef struct {
  unsigned size;
  unsigned char *data;
  unsigned real_size;
  unsigned char *ptr;
  char *name;
  int line;
  int start_offset;
} file_data_t;

struct option options[] = {
  { "create", 1, NULL, 'c' },
  { "show", 0, NULL, 's' },
  { "log", 1, NULL, 'l' },
  { "opt", 1, NULL, 'O' },
  { "debug", 0, NULL, 'g' },
  { "lib", 1, NULL, 'L' },
  { "help", 0, NULL, 'h' },
  { }
};

typedef struct {
  char *name;
  type_t type;
  int line;
  int del, ref, ref_idx, ref_ind, def, def_idx, def_ind, ref0, ref0_idx;
  struct {
    unsigned char *p;
    unsigned u;
    unsigned p_len;
  } value;
} dict_t;

typedef struct {
  char *name;
  type_t type;
  unsigned ofs;
  unsigned size;
  unsigned xref_to;
  unsigned duplicate:1;
  int line, incl_level;
  struct {
    unsigned char *p;
    uint64_t u;
    unsigned p_len;
  } value;
  unsigned char *enc;
} code_t;

void help(void);
file_data_t read_file(char *name);
void fix_pal(unsigned char *pal, unsigned shade1, unsigned shade2, unsigned char *rgb);
int write_data(char *name);
void encode_number(unsigned char *enc, uint64_t val, unsigned len);
uint64_t decode_number(unsigned char *data, unsigned len);
void add_data(file_data_t *d, void *buffer, unsigned size);
code_t *new_code(void);
dict_t *new_dict(void);
int show_info(char *name);
int get_hex(char *s, unsigned len, unsigned *val);
char *utf8_encode(unsigned uc);
int utf8_decode(char **s);
char *utf8_quote(unsigned uc);
char *next_word(char **ptr, int *len);
void parse_comment(char *comment, file_data_t *incl);
int find_in_dict(char *name);
int translate(int pass);
int parse_config(char *name, char *log_file);
void optimize_dict(FILE *lf);
unsigned skip_code(unsigned pos);
unsigned next_code(unsigned pos);
int optimize_code(FILE *lf);
int optimize_code1(FILE *lf);
int optimize_code2(FILE *lf);
int optimize_code3(FILE *lf);
int optimize_code4(FILE *lf);
int optimize_code5(FILE *lf);
int optimize_code6(FILE *lf);
void log_code(FILE *lf, int style);
int decompile(unsigned char *data, unsigned size);

int config_ok = 0;

file_data_t pscode = {};
file_data_t dict_file = {};
file_data_t source_code = {};

dict_t *dict = NULL;
unsigned dict_size = 0;
unsigned dict_max_size = 0;

unsigned prim_words = sizeof prim_names / sizeof *prim_names;

code_t *code = NULL;
unsigned code_size = 0;
unsigned code_max_size = 0;

// current config line
int line = 1;

struct {
  unsigned verbose;
  unsigned optimize;
  unsigned show:1;
  unsigned debug:1;
  char *file;
  char *log_file;
  char *lib_path[2];
} opt = { lib_path: { NULL, "/usr/share/gfxboot" } };

int main(int argc, char **argv)
{
  int i;

  opterr = 0;

  while((i = getopt_long(argc, argv, "c:sfhL:l:O:vg", options, NULL)) != -1) {
    switch(i) {
      case 'c':
        opt.file = optarg;
        break;

      case 's':
        opt.show = 1;
        break;

      case 'g':
        opt.debug = 1;
        break;

      case 'l':
        opt.log_file = optarg;
        break;

      case 'L':
        opt.lib_path[0] = optarg;
        break;

      case 'O':
        opt.optimize = strtoul(optarg, NULL, 0);
        break;

      case 'v':
        opt.verbose++;
        break;

      default:
        help();
        return 0;
    }
  }

  argc -= optind; argv += optind;

  if(opt.file && argc <= 1) {
    if(parse_config(argc ? *argv : "-", opt.log_file)) return 1;
    return write_data(opt.file);
  }

  if(opt.show && argc <= 1) {
    return show_info(argc ? *argv : "-");
  }

  help();

  return 1;
}


void help()
{
  fprintf(stderr, "%s",
    "Usage: gfxboot-compile [OPTIONS] SOURCE\n"
    "Compile/decompile gfxboot2 script to byte code.\n"
    "Options:\n"
    "  -c, --create FILE       Compile SOURCE to FILE.\n"
    "  -l, --log LOGFILE       Write compile log to LOGFILE.\n"
    "  -s, --show              Decompile SOURCE.\n"
    "  -g, --debug             Embed debug info.\n"
    "  -L, --lib PATH          Set include file search path to PATH.\n"
    "  -O, --opt LEVEL         Optimization level (0 - 3).\n"
    "  -v, --verbose           Create more verbose log.\n"
    "  -h, --help              Show this help text.\n"
  );
}


/*
 * The returned buffer has an extra 0 appended to it for easier parsing...
 */
file_data_t read_file(char *name)
{
  file_data_t fd = { };
  FILE *f;
  unsigned u;
  char *s;

  if(!name) return fd;

  if(strcmp(name, "-")) {
    f = fopen(name, "r");
  }
  else {
    f = stdin;
  }

  if(!f) {
    for(u = 0; u < sizeof opt.lib_path / sizeof *opt.lib_path; u++) {
      if(opt.lib_path[u]) {
        asprintf(&s, "%s/%s", opt.lib_path[u], name);
        f = fopen(s, "r");
        if(f) {
          fd.name = s;
          break;
        }
        else {
          free(s);
        }
      }
    }
  }
  else {
    fd.name = strdup(name);
  }
  
  if(!f) { perror(name); return fd; }

  if(fseek(f, 0, SEEK_END)) {
    perror(name);
    exit(30);
  }

  fd.size = fd.real_size = (unsigned) ftell(f);

  if(fseek(f, 0, SEEK_SET)) {
    perror(name);
    exit(30);
  }

  fd.ptr = fd.data = calloc(1, fd.size + 1);
  if(!fd.data) {
    fprintf(stderr, "malloc failed\n");
    exit(30);
  }

  if(fread(fd.data, 1, fd.size, f) != fd.size) {
    perror(name);
    exit(30);
  }

  fclose(f);

  return fd;
}


int write_data(char *name)
{
  FILE *f;
  file_data_t fd = {};

  f = strcmp(name, "-") ? fopen(name, "w") : stdout;

  if(!f) {
    perror(name);
    return 1;
  }

  add_data(&fd, pscode.data, pscode.size);

  if(fwrite(fd.data, fd.size, 1, f) != 1) {
    perror(name);
    return 1;
  }

  fclose(f);

  // FIXME XXXXX
  if(opt.debug) {
    fwrite(source_code.data, source_code.size, 1, stdout);
  }

  return 0;
}


void encode_number(unsigned char *enc, uint64_t val, unsigned len)
{
  while(len--) {
    *enc++ = val;
    val >>= 8;
  }
}


uint64_t decode_number(unsigned char *data, unsigned len)
{
  uint64_t val = 0;

  data += len;

  while(len--) {
    val <<= 8;
    val += *--data;
  }

  return val;
}


void add_data(file_data_t *d, void *buffer, unsigned size)
{
  ssize_t ofs = 0;

  if(!size || !d || !buffer) return;

  if(d->ptr && d->data) ofs = d->ptr - d->data;

  if(d->size + size > d->real_size) {
    d->real_size = d->size + size + 0x1000;
    d->data = realloc(d->data, d->real_size);
    if(!d->data) d->real_size = 0;
  }

  if(d->size + size <= d->real_size) {
    memcpy(d->data + d->size, buffer, size);
    d->size += size;
  }
  else {
    fprintf(stderr, "Oops, out of memory? Aborted.\n");
    exit(10);
  }

  if(d->ptr && d->data) d->ptr = d->data + ofs;
}


code_t *new_code()
{
  if(code_size >= code_max_size) {
    code_max_size += 10;
    code = realloc(code, code_max_size * sizeof * code);
    memset(code + code_size, 0, (code_max_size - code_size) * sizeof * code);
  }

  return code + code_size++;
}



dict_t *new_dict()
{
  if(dict_size >= dict_max_size) {
    dict_max_size += 10;
    dict = realloc(dict, dict_max_size * sizeof *dict);
    memset(dict + dict_size, 0, (dict_max_size - dict_size) * sizeof *dict);
  }

  return dict + dict_size++;
}


uint32_t read_uint32_le(file_data_t *fd, unsigned ofs)
{
  uint32_t word = 0;
  unsigned u;
  for (u = 0; u < 4; u++) {
    word += (unsigned) fd->data[ofs + u] << (u * 8);
  }
  return word;
}


int show_info(char *name)
{
  file_data_t fd;
  int err = 1;

  fd = read_file(name);

  err = decompile(fd.data, fd.size);
  if(!err) {
    log_code(stdout, 0);
  }

  return err;
}


/*
 * Convert hex number of excatly len bytes.
 */
int get_hex(char *s, unsigned len, unsigned *val)
{
  unsigned u;
  char s2[len + 1];

  if(!s || !len) return 0;
  strncpy(s2, s, len);
  s2[len] = 0;

  u = strtoul(s2, &s, 16);
  if(!*s) {
    if(val) *val = u;
    return 1;
  }

  return 0;
}


char *utf8_encode(unsigned uc)
{
  static char buf[7];
  char *s = buf;

  uc &= 0x7fffffff;

  if(uc < 0x80) {			// 7 bits
    *s++ = uc;
  }
  else {
    if(uc < (1 << 11)) {		// 11 (5 + 6) bits
      *s++ = 0xc0 + (uc >> 6);
      goto utf8_encode_2;
    }
    else if(uc < (1 << 16)) {		// 16 (4 + 6 + 6) bits
      *s++ = 0xe0 + (uc >> 12);
      goto utf8_encode_3;
    }
    else if(uc < (1 << 21)) {		// 21 (3 + 6 + 6 + 6) bits
      *s++ = 0xf0 + (uc >> 18);
      goto utf8_encode_4;
    }
    else if(uc < (1 << 26)) {		// 26 (2 + 6 + 6 + 6 + 6) bits
      *s++ = 0xf8 + (uc >> 24);
      goto utf8_encode_5;
    }
    else {				// 31 (1 + 6 + 6 + 6 + 6 + 6) bits
      *s++ = 0xfc + (uc >> 30);
    }

    *s++ = 0x80 + ((uc >> 24) & ((1 << 6) - 1));

    utf8_encode_5:
      *s++ = 0x80 + ((uc >> 18) & ((1 << 6) - 1));

    utf8_encode_4:
      *s++ = 0x80 + ((uc >> 12) & ((1 << 6) - 1));

    utf8_encode_3:
      *s++ = 0x80 + ((uc >> 6) & ((1 << 6) - 1));

    utf8_encode_2:
      *s++ = 0x80 + (uc & ((1 << 6) - 1));
  }

  *s = 0;

  return buf;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Decode utf8 sequence.
//
// a) if s points to a valid utf8 sequence:
//  - returns unicode char (a non-negative number)
//  - s is updated to point past utf8 char
//
// b) if s does not point to a valid utf8 sequence
//  - returns negated first byte
//  - s is incremented by 1
//
int utf8_decode(char **s)
{
  unsigned char *p;
  int c;
  unsigned u, l;

  if(!s || !*s) return 0;

  p = (uint8_t *) *s;

  u = *p++;

  if(u >= 0x80) {
    if(u < 0xc0 || u >= 0xfe) {
      *s = (char *) p;
      return -(int) u;
    }
    l = 1;
    if(u < 0xe0) {
      c = u & 0x1f;
    }
    else if(u < 0xf0) {
      c = u & 0x0f;
      l = 2;
    }
    else if(u < 0xf8) {
      c = u & 0x07;
      l = 3;
    }
    else if(u < 0xfc) {
      c = u & 0x03;
      l = 4;
    }
    else if(u < 0xfe) {
      c = u & 0x01;
      l = 5;
    }
    while(l--) {
      u = *p++;
      if(u < 0x80 || u >= 0xc0) {
        u = (uint8_t) **s;
        (*s)++;
        return -(int) u;
      }
      c = (c << 6) + (int) (u & 0x3f);
    }
  }
  else {
    c = (int) u;
  }

  *s = (char *) p;

  return c;
}


char *utf8_quote(unsigned uc)
{
  static char buf[16];

  if(uc == '\t') {
    strcpy(buf, "\\t");
  }
  else if(uc == '\n') {
    strcpy(buf, "\\n");
  }
  else if(uc == '\\') {
    strcpy(buf, "\\\\");
  }
  else if(uc == '\'') {
    strcpy(buf, "\\'");
  }
  else if(uc < ' ' || uc == 0x7f) {
    sprintf(buf, "\\x%02x", uc);
  }
  else if(uc >= ' ' && uc < 0x7f) {
    buf[0] = uc;
    buf[1] = 0;
  }
  else if(uc < 0xffff) {
    sprintf(buf, "\\u%04x", uc);
  }
  else {
    sprintf(buf, "\\U%08x", uc);
  }

  return buf;
}


char *next_word(char **ptr, int *len)
{
  char *s, *start, *utf8;
  int is_str, is_comment;
  static char word[0x1000];
  unsigned u, n = 0;
  char qc = 0;

  s = *ptr;

  *word = 0;

  if(len) *len = (int) n;

  while(*s && isspace(*s)) if(*s++ == '\n') line++;

  if(!*s) {
    *ptr = s;
    return word;
  }

  start = s;

  qc = *start;
  is_str = qc == '"' || qc == '\'' ? 1 : 0;
  is_comment = qc == COMMENT_CHAR ? 1 : 0;

  if(is_comment) {
    while(*s && *s != '\n') s++;
  }
  else if(is_str) {
    *word = *s++;
    for(n = 1; n < sizeof word - 1; n++) {
      if(!*s) break;
      if(*s == qc) { s++; break; }
      if(*s == '\\') {
        s++;
        switch(*s) {
          case 0:
            word[n++] = '\\';
            break;

          case 'n':
            word[n] = '\n';
            break;
          
          case 't':
            word[n] = '\t';
            break;
          
          case '0':
            if(
              s[0] >= '0' && s[0] <= '7' &&
              s[1] >= '0' && s[1] <= '7' &&
              s[2] >= '0' && s[2] <= '7'
            ) {
              word[n] = ((s[0] - '0') << 6) + ((s[1] - '0') << 3) + (s[2] - '0');
              s += 2;
            }
            else {
              word[n] = *s;
            }
            break;
          
          case 'x':
            if(get_hex(s + 1, 2, &u)) {
              s += 2;
              word[n] = u;
            }
            else {
              word[n++] = '\\';
              word[n] = *s;
            }
            break;
          
          case 'u':
            if(get_hex(s + 1, 4, &u)) {
              s += 4;
              utf8 = utf8_encode(u);
              while(*utf8) word[n++] = *utf8++;
              n--;
            }
            else {
              word[n++] = '\\';
              word[n] = *s;
            }
            break;
          
          case 'U':
            if(get_hex(s + 1, 8, &u)) {
              s += 8;
              utf8 = utf8_encode(u);
              while(*utf8) word[n++] = *utf8++;
              n--;
            }
            else {
              word[n++] = '\\';
              word[n] = *s;
            }
            break;
          
          default:
            word[n] = *s;
        }
        s++;
      }
      else {
        word[n] = *s++;
      }
    }
    word[n] = 0;
  }
  else {
    while(*s && !isspace(*s)) s++;
  }

  if(!is_str) {
    n = (unsigned) (s - start);
    if(n >= sizeof word) n = sizeof word - 1;
    strncpy(word, start, n);
    word[n] = 0;
  }

  *ptr = s;

  if(len) *len = (int) n;

  return word;
}


void parse_comment(char *comment, file_data_t *incl)
{
  char t[5][100];
  int n;

  n = sscanf(comment, " %99s %99s %99s %99s %99s", t[0], t[1], t[2], t[3], t[4]);

  if(!n) return;

  if(n == 2 && !strcmp(t[0], "include")) {
    *incl = read_file(t[1]);
    if(!incl->data) exit(18);
    add_data(incl, "", 1);
    if(opt.verbose) fprintf(stderr, "including \"%s\"\n", incl->name);
    return;
  }
}


int find_in_dict(char *name)
{
  unsigned u;

  for(u = 0; u < dict_size; u++) {
    if(dict[u].name && !strcmp(name, dict[u].name)) return (int) u;
  }

  return -1;
}


unsigned usize(uint64_t val)
{
  unsigned len = 0;

  while(val) {
    val >>= 8;
    len++;
  }

  return len;
}


unsigned isize(int64_t val)
{
  unsigned len = 1;

  if(val == 0) return 0;

  while((val >> 7) && (val >> 7) != -1L) {
    val >>= 8;
    len++;
  };

  return len;
}


int translate(int pass)
{
  int is_signed;
  code_t *c;
  unsigned u, ofs = 0, len, lenx;
  int changed = 0;

  if(pass == 0) {
    changed = 1;
    for(u = 0; u < code_size; u++) {
      c = code + u;

      c->ofs = ofs;

      is_signed = 0;
      switch(c->type) {
        case t_skip:
          c->size = 0;
          break;

        case t_int:
          is_signed = 1;

        case t_nil:
        case t_bool:
        case t_prim:
        case t_comment:
        case t_string:
        case t_word:
        case t_ref:
        case t_get:
        case t_set:
          if(c->value.p) {
            if(!TYPE_EXPECTS_DATA(c->type)) {
              fprintf(stderr, "Internal oops %d: type %d needs memory range\n", __LINE__, c->type);
              exit(9);
            }
            lenx = c->value.p_len;
            if(lenx < 12) {
              c->size = lenx + 1;
              c->enc = malloc(c->size);
              memcpy(c->enc + 1, c->value.p, lenx);
              c->enc[0] = c->type + (lenx << 4);
            }
            else {
              // len is at least 1 since lenx is != 0
              len = usize(lenx);
              if(len > 4) {
                fprintf(stderr, "Internal oops %d: type %d memory range is too large\n", __LINE__, c->type);
                exit(11);
              }
              c->size = lenx + len + 1;
              c->enc = malloc(c->size);
              c->enc[0] = c->type + ((len + 11) << 4);
              encode_number(c->enc + 1, lenx, len);
              memcpy(c->enc + 1 + len, c->value.p, lenx);
            }
          }
          else {
            if(TYPE_EXPECTS_DATA(c->type)) {
              fprintf(stderr, "Internal oops %d: type %d misses memory range\n", __LINE__, c->type);
              exit(10);
            }
            len = is_signed ? isize((int64_t) c->value.u) : usize(c->value.u);
            if(c->value.u < 8) {
              c->size = 1;
              c->enc = malloc(c->size);
              c->enc[0] = c->type + (c->value.u << 4);
            }
            else {
              // len is at least 1 since c->value.u is != 0
              c->size = len + 1;
              c->enc = malloc(c->size);
              c->enc[0] = c->type + ((len - 1 + 8) << 4);
              encode_number(c->enc + 1, c->value.u, len);
            }
          }
          break;

        case t_code:
          c->size = 2;
          // dummy value
          // really encoded in else branch below during later passes (pass != 0)
          break;

        default:
          fprintf(stderr, "Internal oops %d: type %d not allowed\n", __LINE__, c->type);
          exit(8);
      }

      ofs += c->size;
    }
  }
  else {
    for(u = 0; u < code_size; u++) {
      c = code + u;

      if(c->ofs != ofs) changed = 1;
      c->ofs = ofs;

      if(c->xref_to) {
        unsigned dist = c->ofs - code[c->xref_to].ofs;
        unsigned xlen = usize(dist);
        if(dist < 8) {
          if(1 < c->size || c->type == t_xref) {
            c->size = 1;
            c->enc = malloc(c->size);
            c->enc[0] = t_xref + (dist << 4);
            c->type = t_xref;
          }
        }
        else {
          if(xlen + 1 < c->size || c->type == t_xref) {
            c->enc[0] = t_xref + ((xlen - 1 + 8) << 4);
            encode_number(c->enc + 1, dist, xlen);
            c->size = xlen + 1;
            c->type = t_xref;
          }
        }
      }

      if(c->type == t_code) {
        lenx = c->value.u;
        // we want to encode the length of the code blob, starting *after*
        // the current instruction
        if(lenx >= code_size || u >= code_size - 1 || code[lenx].ofs < c[1].ofs) {
          fprintf(stderr, "Internal error %d\n", __LINE__);
          exit(11);
        }
        lenx = code[lenx].ofs - c[1].ofs;
        if(lenx < 12) {
          c->size = 1;
          c->enc = malloc(c->size);
          c->enc[0] = c->type + (lenx << 4);
        }
        else {
          len = usize(lenx);
          if(c->size != len + 1) changed = 1;
          c->size = len + 1;
          if(c->enc) free(c->enc);
          c->enc = malloc(c->size);
          c->enc[0] = c->type + ((len + 11) << 4);
          encode_number(c->enc + 1, lenx, len);
        }
      }

      ofs += c->size;
    }
  }

  return changed;
}


int parse_config(char *name, char *log_file)
{
  char *word;
  file_data_t cfg[MAX_INCLUDE];
  file_data_t incl;
  int i, j;
  unsigned u, word_len;
  dict_t *d;
  code_t *c, *c1;
  char *s;
  FILE *lf = NULL;
  int incl_level = 0;

  cfg[incl_level] = read_file(name);
  add_data(&cfg[incl_level], "", 1);

  if(!cfg[incl_level].ptr) {
    fprintf(stderr, "error: no source file\n");

    return 1;
  }

  if(log_file && *log_file) {
    if(!strcmp(log_file, "-")) {
      lf = fdopen(dup(fileno(stdout)), "a");
    }
    else {
      lf = fopen(log_file, "w");
    }
  }

  add_data(&source_code, cfg[incl_level].data, cfg[incl_level].size);

  // setup initial vocabulary
  for(u = 0; u < prim_words; u++) {
    d = new_dict();
    d->type = t_prim;
    d->value.u = u;
    d->name = (char *) prim_names[u];
  }

  c = new_code();
  c->type = t_comment;
  c->value.p_len = 7;
  c->value.p = calloc(1, 7);
  encode_number(c->value.p, GFXBOOT_MAGIC, 7);
  c->name = strdup("# gfxboot magic");

  while(*cfg[incl_level].ptr || incl_level) {
    if(!*cfg[incl_level].ptr) {
      incl_level--;
      line = cfg[incl_level].line;
    }
    word = next_word((char **) &cfg[incl_level].ptr, &word_len);	// unsigned char **
    if(!word || !word_len) continue;

    if(word[0] == COMMENT_CHAR) {
      if(word[1] == COMMENT_CHAR) {
        incl.ptr = NULL;
        parse_comment(word + 2, &incl);
        if(incl.ptr) {
          if(incl_level == MAX_INCLUDE - 1) {
            fprintf(stderr, "error: include level exceeded\n");
            return 1;
          }
          else {
            incl.start_offset = source_code.size;
            add_data(&source_code, incl.data, incl.size);
            cfg[incl_level].line = line;
            cfg[++incl_level] = incl;
            line = 1;
          }
        }
      }
      continue;
    }

    if(opt.verbose >= 2) printf(">%s< [%d] (line %d - start %d)\n", word, word_len, line, cfg[incl_level].start_offset);

    c = new_code();
    c->line = line;
    c->incl_level = incl_level;

    if(*word == '"') {
      c->type = t_string;
      c->value.p = calloc(1, word_len);
      c->value.p = memcpy(c->value.p, word + 1, word_len - 1);
      c->value.p_len = word_len - 1;
    }
    else if(*word == '\'') {
      char *s = word + 1;
      int uc = utf8_decode(&s);
      if(uc >= 0 && !*s) {
        c->type = t_int;
        c->value.u = (unsigned) uc;
        asprintf(&c->name, "'%s'", utf8_quote((unsigned) uc));
      }
      else {
        fprintf(stderr, "syntax error: invalid char constant in line %d\n", line);
        return 1;
      }
    }
    else if(*word == '/') {
      c->name = strdup(word + 1);

      c->type = t_ref;

      if((i = find_in_dict(word + 1)) == -1) {
        d = new_dict();
        d->type = t_nil;
        d->value.u = 1;		// mark as defined
        d->value.p = strdup(word + 1);
        d->value.p_len = strlen(word + 1);
        d->name = strdup(word + 1);
        c->value.u = dict_size - 1;
      }
      else {
        if(dict[i].type == t_nil && !dict[i].value.u) {
          dict[i].value.u = 1;	// mark as defined
        }
        c->value.u = (unsigned) i;
      }
      c->value.p = strdup(word + 1);
      c->value.p_len = strlen(word + 1);
    }
    else if(*word == '.') {
      c->name = strdup(word + 1);
      c->type = t_get;
      c->value.p = strdup(word + 1);
      c->value.p_len = strlen(word + 1);
    }
    else if(*word == '=') {
      c->name = strdup(word + 1);
      c->type = t_set;
      c->value.p = strdup(word + 1);
      c->value.p_len = strlen(word + 1);
    }
    else if(!strcmp(word, prim_names[prim_idx_code_start])) {
      c->type = t_code;
      c->name = strdup(word);
    }
    else if(!strcmp(word, prim_names[prim_idx_code_end])) {
      c->type = t_prim;
      c->value.u = prim_idx_code_end;
      c->name = strdup(word);
      for(c1 = c; c1 >= code; c1--) {
        if(c1->type == t_code && !c1->value.u) {
          // point _after_ "}"
          c1->value.u = (unsigned) (c - code) + 1;
          break;
        }
      }
      if(c1 < code) {
        fprintf(stderr, "syntax error: no matching \"{\" for \"}\" in line %d\n", line);
        return 1;
      }
    }
    else {
      c->name = strdup(word);

      i = find_in_dict(word);

      if(i == -1) {
        uint64_t val = strtoull(word, &s, 0);
        if(*s) {
          if(!strcmp(s, "nil")) {
            c->type = t_nil;
            c->value.u = 0;
          }
          else if(!strcmp(s, "true")) {
            c->type = t_bool;
            c->value.u = 1;
          }
          else if(!strcmp(s, "false")) {
            c->type = t_bool;
            c->value.u = 0;
          }
          else {
            d = new_dict();
            d->type = t_nil;
            d->name = strdup(word);
            c->type = t_word;
            c->value.u = dict_size - 1;
            c->value.p = strdup(word);
            c->value.p_len = strlen(word);
          }
        }
        else {
          c->type = t_int;
          c->value.u = val;
        }
      }
      else {
        c->type = t_word;
        c->value.u = (unsigned) i;
        c->value.p = strdup(word);
        c->value.p_len = strlen(word);
      }
    }
  }

  // check vocabulary
  if(opt.verbose >= 2) {
    for(i = j = 0; i < (int) dict_size; i++) {
      if(
        dict[i].type == t_nil && !dict[i].value.u
      ) {
        if(!j) fprintf(stderr, "Undefined words:");
        else fprintf(stderr, ",");
        fprintf(stderr, " %s", dict[i].name);
        j = 1;
      }
    }
    if(j) {
      fprintf(stderr, "\n");
    }
  }

  if(opt.optimize) {
    if(opt.verbose >= 2 && lf) fprintf(lf, "# searching for unused code:\n");
    for(i = 0; i < 64; i++) {
      if(opt.verbose >= 2 && lf) fprintf(lf, "# pass %d\n", i + 1);
      if(!optimize_code(lf)) break;
    }
    if(opt.verbose >= 2 && lf) fprintf(lf, "# %d optimization passes\n", i + 1);
    if(i) {
      if(opt.verbose >= 2 && lf) fprintf(lf, "# searching for unused dictionary entries:\n");
      optimize_dict(lf);
    }
  }

  // translate to byte code
  for(i = 0; i < 100; i++) {
    if(!translate(i)) break;
  }
  if(opt.verbose >= 2 && lf) fprintf(lf, "# %d encoding passes\n", i + 1);
  if(i == 100) {
    fprintf(stderr, "error: code translation does not converge\n");
    return 1;
  }

  // store it
  for(i = 0; i < (int) code_size; i++) {
    if((!code[i].enc || !code[i].size) && code[i].type != t_skip) {
      fprintf(stderr, "error: internal oops %d\n", __LINE__);
      return 1;
    }
    add_data(&pscode, code[i].enc, code[i].size);
  }

  if(lf) fputc('\n', lf);
  log_code(lf, 1);

  if(lf) fclose(lf);

  return 0;
}


/*
 * Remove deleted dictionary entries.
 */
void optimize_dict(FILE *lf)
{
  unsigned u, old_ofs, new_ofs;

  for(old_ofs = new_ofs = 0; old_ofs < dict_size; old_ofs++) {
    if(dict[old_ofs].del) continue;
    if(old_ofs != new_ofs) {
      if(opt.verbose >= 2 && lf) fprintf(lf, "#   rename %d -> %d\n", old_ofs, new_ofs);
      dict[new_ofs] = dict[old_ofs];
      for(u = 0; u < code_size; u++) {
        if(
          (
            code[u].type == t_word ||
            code[u].type == t_ref
          ) &&
          code[u].value.u == old_ofs
        ) {
          code[u].value.u = new_ofs;
        }
      }
    }
    new_ofs++;
  }
  if(opt.verbose >= 2 && lf && new_ofs != old_ofs) {
    fprintf(lf, "# new dictionary size %d (%d - %d)\n", new_ofs, old_ofs, old_ofs - new_ofs);
  }

  dict_size = new_ofs;
}


/*
 * Skip deleted code.
 */
unsigned skip_code(unsigned pos)
{
  while(pos < code_size && code[pos].type == t_skip) pos++;

  return pos;
}


/*
 * Return next instruction.
 */
unsigned next_code(unsigned pos)
{
  if((pos + 1) >= code_size) return pos;

  return skip_code(++pos);
}


int optimize_code(FILE *lf)
{
  unsigned u;
  int changed = 0, ind = 0;
  code_t *c;

  for(u = 0; u < dict_size; u++) {
    dict[u].def = dict[u].def_idx =
    dict[u].ref = dict[u].ref_idx =
    dict[u].ref0 =  dict[u].ref0_idx = 0;
  }

  for(u = 0; u < code_size; u++) {
    c = code + u;

    switch(c->type) {
      case t_code:
        ind++;
        break;

      case t_prim:
        if(c->value.u == prim_idx_code_end) {
          if(!ind) {
            fprintf(stderr, "Warning: nesting error at line %d\n", c->line);
          }
          ind--;
        }
        break;

      case t_word:
        if(c->value.u < dict_size) {
          dict[c->value.u].ref++;
          dict[c->value.u].ref_idx = (int) u;
          dict[c->value.u].ref_ind = ind;
          if(ind == 0 && !dict[c->value.u].ref0) {
            dict[c->value.u].ref0 = 1;
            dict[c->value.u].ref0_idx = (int) u;
          }
        }
        break;

      case t_ref:
        if(c->value.u < dict_size) {
          dict[c->value.u].def++;
          dict[c->value.u].def_idx = (int) u;
          dict[c->value.u].def_ind = ind;
        }
        break;

      default:
        break;
    }
  }

  if(opt.optimize >= 2) changed |= optimize_code1(lf);
  if(opt.optimize >= 3) changed |= optimize_code2(lf);
  if(opt.optimize >= 3) changed |= optimize_code3(lf);
  if(opt.optimize >= 3) changed |= optimize_code5(lf);
  if(opt.optimize >= 2) changed |= optimize_code4(lf);
  // must always be the last step
  if(opt.optimize >= 1) changed |= optimize_code6(lf);

  return changed;
}


/*
 * Find references to primitive words.
 */
int optimize_code1(FILE *lf)
{
  unsigned i, j;
  int changed = 0;
  code_t *c;

  for(i = 0; i < dict_size; i++) {
    if(
      i < prim_words &&
      !dict[i].del &&
      dict[i].def == 0 &&
      dict[i].ref &&
      dict[i].type == t_prim
    ) {
      if(opt.verbose >= 2 && lf) fprintf(lf, "#   replacing %s\n", dict[i].name);
      for(j = 0; j < code_size; j++) {
        c = code + j;
        if(c->type == t_word && c->value.u == i) {
          c->type = dict[i].type;
          c->value.u = dict[i].value.u;
          c->value.p = NULL;
          c->value.p_len = 0;
        }
      }

      changed = 1;
    }
  }

  return changed;
}


/*
 * Remove things like
 *
 *   /foo 123 def
 *   /foo "abc" def
 *   /foo /bar def
 *
 * if foo is unused.
 */
int optimize_code2(FILE *lf)
{
  unsigned i, j;
  int changed = 0;
  code_t *c0, *c1, *c2;

  for(i = 0; i < dict_size; i++) {
    if(
      i >= prim_words &&
      !dict[i].del &&
      !dict[i].ref &&
      dict[i].def == 1 &&
      dict[i].type == t_nil
    ) {
      c0 = code + (j = (unsigned) dict[i].def_idx);
      c1 = code + (j = next_code(j));
      c2 = code + (j = next_code(j));

      if(
        c0->type == t_ref &&
        c0->value.u == i &&
        (
          c1->type == t_nil ||
          c1->type == t_int ||
          c1->type == t_bool ||
          c1->type == t_string ||
          c1->type == t_ref
        ) &&
        c2->type == t_prim &&
        !strcmp(dict[c2->value.u].name, "def")
      ) {
        if(opt.verbose >= 2 && lf) fprintf(lf, "#   defined but unused: %s (index %d)\n", dict[i].name, i);
        if(opt.verbose >= 2 && lf) fprintf(lf, "#   deleting code: %d - %d\n", dict[i].def_idx, j);
        c0->type = c1->type = c2->type = t_skip;
        dict[i].del = 1;

        changed = 1;
      }
    }
  }

  return changed;
}


/*
 * Remove things like
 *
 *   /foo { ... } def
 *
 * if foo is unused.
 */
int optimize_code3(FILE *lf)
{
  unsigned i, j, k;
  int changed = 0;
  code_t *c0, *c1;

  for(i = 0; i < dict_size; i++) {
    if(
      i >= prim_words &&
      !dict[i].del &&
      !dict[i].ref &&
      dict[i].def == 1 &&
      dict[i].type == t_nil
    ) {
      c0 = code + (j = (unsigned) dict[i].def_idx);
      c1 = code + next_code(j);

      if(c1 == c0) continue;

      if(
        c0->type == t_ref &&
        c0->value.u == i &&
        c1->type == t_code &&
        code[j = skip_code(c1->value.u)].type == t_prim &&
        !strcmp(dict[code[j].value.u].name, "def") &&
        j > (unsigned) dict[i].def_idx
      ) {
        if(opt.verbose >= 2 && lf) fprintf(lf, "#   defined but unused: %s (index %d)\n", dict[i].name, i);
        if(opt.verbose >= 2 && lf) fprintf(lf, "#   deleting code: %d - %d\n", dict[i].def_idx, j);
        for(k = (unsigned) dict[i].def_idx; k <= j; k++) code[k].type = t_skip;
        dict[i].del = 1;

        changed = 1;
      }
    }
  }

  return changed;
}



/*
 * Find unused dictionary entries.
 */
int optimize_code4(FILE *lf)
{
  unsigned i;
  int changed = 0;

  for(i = 0; i < dict_size; i++) {
    if(
      i >= prim_words &&
      !dict[i].del &&
      !dict[i].ref &&
      !dict[i].def
    ) {
      if(opt.verbose >= 2 && lf) fprintf(lf, "#   unused: %s (index %d)\n", dict[i].name, i);

      dict[i].del = 1;

      changed = 1;
    }
  }

  return changed;
}


/*
 * Replace references to constant global vars.
 */
int optimize_code5(FILE *lf)
{
  unsigned i, j, k;
  int changed = 0;
  code_t *c, *c0, *c1, *c2;
  char *s;

  for(i = 0; i < dict_size; i++) {
    if(
      i >= prim_words &&
      !dict[i].del &&
      dict[i].def == 1 &&
      dict[i].def_ind == 0 &&
      (
        !dict[i].ref0 ||
        dict[i].ref0_idx >  dict[i].def_idx
      ) &&
      dict[i].type == t_nil
    ) {
      c0 = code + (j = (unsigned) dict[i].def_idx);
      c1 = code + (j = next_code(j));
      c2 = code + (j = next_code(j));

      if(
        c0->type == t_ref &&
        c0->value.u == i &&
        (
          c1->type == t_nil ||
          c1->type == t_int ||
          c1->type == t_bool
        ) &&
        c2->type == t_prim &&
        !strcmp(dict[c2->value.u].name, "def")
      ) {
        if(opt.verbose >= 2 && lf) fprintf(lf, "#   global constant: %s (index %d)\n", dict[i].name, i);
        if(opt.verbose >= 2 && lf) fprintf(lf, "#   replacing %s with %s\n", dict[i].name, c1->name);
        for(k = 0; k < code_size; k++) {
          c = code + k;
          if(c->type == t_word && c->value.u == i) {
            c->type = c1->type;
            c->value = c1->value;
            if(c->type == t_int) {
              asprintf(&s, "%s # %s", c1->name, c->name);
              free(c->name);
              c->name = s;
            }
            else if(c->type == t_bool) {
              asprintf(&s, "%s # %s", c->value.u ? "true" : "false", c->name);
              free(c->name);
              c->name = s;
            }
            else if(c->type == t_nil) {
              asprintf(&s, "nil # %s", c->name);
              free(c->name);
              c->name = s;
            }
          }
        }

        dict[i].del = 1;

        if(opt.verbose >= 2 && lf) fprintf(lf, "#   deleting code: %d - %d\n", dict[i].def_idx, j);
        c0->type = c1->type = c2->type = t_skip;

        changed = 1;
      }
    }
  }

  return changed;
}


/*
 */
int optimize_code6(FILE *lf)
{
  unsigned i, j;
  int changed = 0;
  code_t *c, *c_ref;

  if(opt.verbose >= 2 && lf) fprintf(lf, "#   looking for cross references\n");

  for(i = 0; i < code_size; i++) {
    c_ref = code + i;
    if(c_ref->xref_to) continue;
    if(
      c_ref->type == t_string ||
      c_ref->type == t_word ||
      c_ref->type == t_ref ||
      c_ref->type == t_get ||
      c_ref->type == t_set
    ) {
      for(j = i + 1; j < code_size; j++) {
        c = code + j;
        if(
          c->type == c_ref->type &&
          c_ref->value.p && c->value.p &&
          c_ref->value.p_len == c->value.p_len &&
          !memcmp(c->value.p, c_ref->value.p, c->value.p_len)
        ) {
          c->xref_to = i;
          if(opt.verbose >= 2 && lf) fprintf(lf, "xref: %d = %d name = >%s<\n", j, i, c->name);
          changed = 1;
        }
      }
    }
  }

  return changed;
}


void log_code(FILE *lf, int style)
{
  int i, j, l, line = 0, incl_level = 0;
  int ind = 0, diff = 0;
  char *s;

  if(!lf) return;

  for(i = j = 0; i < (int) code_size; i++) {
    if(code[i].type == t_skip) j++;
  }

  fprintf(lf, "# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
  fprintf(lf, "# code: %d entries (%d - %d)\n", (int) code_size - j, code_size, j);
  if(style || opt.verbose) {
    fprintf(lf, "# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
    fprintf(lf, "# line i %soffset   type   hex                      word\n", opt.verbose ? "index  " : "");
  }
  fprintf(lf, "# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
  for(i = 0; i < (int) code_size; i++) {
    if(code[i].duplicate) {
      diff++;
    }
    if(code[i].type == t_skip && !opt.verbose) continue;
    if(style || opt.verbose) {
      if((line != code[i].line || incl_level != code[i].incl_level) && code[i].line) {
        line = code[i].line;
        incl_level = code[i].incl_level;
        fprintf(lf, "%6d", line);
        if(incl_level) {
          fprintf(lf, " %d ", incl_level);
        }
        else {
           fprintf(lf, "   ");
        }
      }
      else {
        fprintf(lf, "%9s", "");
      }
      if(opt.verbose) {
        if(code[i].duplicate) {
          fprintf(lf, "%5s  ", "");
        }
        else {
          fprintf(lf, "%5d  ", i - diff);
        }
      }
      if(code[i].size && !code[i].duplicate) {
        fprintf(lf, "0x%05x  ", code[i].ofs);
      }
      else {
        fprintf(lf, "%*s", 9, "");
      }
      if(code[i].type < sizeof type_name / sizeof *type_name) {
        fprintf(lf, "%-6s", type_name[code[i].type]);
      }
      else {
        fprintf(lf, "<%4u>", code[i].type);
      }
      l = code[i].enc ? (int) code[i].size : 0;
      if(l > 8) l = 8;
      for(j = 0; j < l; j++) {
        fprintf(lf, " %02x", code[i].enc[j]);
      }
    }
    else {
      l = 8;
    }

    type_t type = code[i].type;
    if(code[i].xref_to) type = code[code[i].xref_to].type;

    if(
      (
        (type == t_word || type == t_prim) &&
        (
          !strcmp(code[i].name, prim_names[prim_idx_array_end]) ||
          !strcmp(code[i].name, prim_names[prim_idx_hash_end]) ||
          !strcmp(code[i].name, prim_names[prim_idx_code_end])
        )
      ) &&
      ind > 0
    ) ind -= 2;
    fprintf(lf, "%*s", 3 * (8 - l) + ind + (style || opt.verbose ? 2 : 0), "");

    if(type == t_skip) fprintf(lf, "# ");

    if(
      type == t_code ||
      (
        (type == t_word || type == t_prim) &&
        (
          !strcmp(code[i].name, prim_names[prim_idx_array_start]) ||
          !strcmp(code[i].name, prim_names[prim_idx_hash_start]) ||
          !strcmp(code[i].name, prim_names[prim_idx_code_start])
        )
      )
    ) ind += 2;

    if(
      type == t_string ||
      type == t_word ||
      type == t_ref ||
      type == t_get ||
      type == t_set
    ) {
      if(type == t_string) fprintf(lf, "\"");
      if(type == t_ref) fprintf(lf, "/");
      if(type == t_get) fprintf(lf, ".");
      if(type == t_set) fprintf(lf, "=");
      s = code[i].value.p;
      unsigned p_len = code[i].value.p_len;
      while(p_len--) {
        if(*s >= 0 && *s < 0x20) {
          if(*s == '\n') {
            fprintf(lf, "\\n");
          }
          else if(*s == '\t') {
            fprintf(lf, "\\t");
          }
          else {
            fprintf(lf, "\\x%02x", (unsigned char) *s);
          }
        }
        else {
          fprintf(lf, "%c", *s);
        }
        s++;
      }
      if(type == t_string) fprintf(lf, "\"");
    }
    else {
      fprintf(lf, "%s", code[i].name ?: "");
    }

    if((style || opt.verbose) && code[i].enc && code[i].size > 8) {
      for(j = 8; j < (int) code[i].size; j++) {
        if(j & 7) {
          fprintf(lf, " ");
        }
        else {
          fprintf(lf, "\n%*s", 25 + (opt.verbose ? 7 : 0), "");
        }
        fprintf(lf, "%02x", code[i].enc[j]);
      }
    }
    fprintf(lf, "\n");
  }
}


unsigned decode_instr(unsigned char *data, type_t *type, int64_t *arg1, unsigned char **arg2)
{
  unsigned u, len;
  type_t t;
  unsigned inst_size = 1;
  int64_t val;

  u = data[0] >> 4;
  *type = t = data[0] & 0xf;
  *arg2 = 0;

  if(TYPE_EXPECTS_DATA(t)) {
    if(u >= 12) {
      u -= 11;
      len = decode_number(data + 1, u);
      inst_size += u;
    }
    else {
      len = u;
    }
    *arg1 = len;
    // we want to decode the code blobs
    if(t != t_code) {
      *arg2 = data + inst_size;
      inst_size += len;
    }
  }
  else {
    if(u >= 8) {
      u -= 7;
      val = (int64_t) decode_number(data + 1, u);
      inst_size += u;
      if(t == t_int) {
        // expand sign bit
        val <<= 8 * (8 - u);
        val >>= 8 * (8 - u);
      }
    }
    else {
      val = u;
    }
    *arg1 = val;
  }

  return inst_size;
}


int decompile(unsigned char *data, unsigned size)
{
  unsigned i, j, inst_size;
  dict_t *d;
  code_t *c;
  type_t type;
  int64_t arg1;
  unsigned char *arg2;

  // setup initial vocabulary
  for(i = 0; i < prim_words; i++) {
    d = new_dict();
    d->type = t_prim;
    d->value.u = i;
    d->name = (char *) prim_names[i];
  }

  for(i = 0; i < size; i += inst_size) {
    inst_size = decode_instr(data + i, &type, &arg1, &arg2);

    if(i + inst_size > size) {
      if(i) {
        fprintf(stderr, "error: instruction size bounds exceeded: %u > %u\n", i + inst_size, size);
      }
      else {
        fprintf(stderr, "error: invalid file format\n");
      }

      return 1;
    }

    c = new_code();
    c->type = type;

    if(
      i == 0 &&
      !(
        c->type == t_comment &&
        inst_size == 8
      )
    ) {
      fprintf(stderr, "error: invalid file format\n");

      return 1;
    }

    c->ofs = i;
    c->size = inst_size;
    c->enc = malloc(inst_size);
    memcpy(c->enc, data + i, inst_size);

    c->value.u = (uint64_t) arg1;

    if(arg2) {
      c->value.p = arg2;
      c->value.p_len = arg1;
    }

    if(i == 0 && decode_number(c->value.p, c->value.p_len) != GFXBOOT_MAGIC) {
      fprintf(stderr, "error: gfxboot magic not matching\n");

      return 1;
    }

    if(c->type == t_xref) {
      c->value.u = (uint64_t) arg1;
      for(j = 0; j < code_size - 1; j++) {
        if(code[j].ofs == c->ofs - arg1) {
          c->xref_to = j;
        }
      }
      if(!c->xref_to) {
        fprintf(stderr, "error: invalid cross reference: ofs 0x%x, %d\n", c->ofs, (unsigned) arg1);
        return 2;
      }
      code_t *c_ref = code + c->xref_to;
      unsigned old_ofs = c->ofs;
      c->xref_to = 0;
      asprintf(&c->name, "# -> offset 0x%05x", c_ref->ofs);
      if(opt.verbose >= 2) {
        c = new_code();
        *c = *c_ref;
        c->duplicate = 1;
      }
      else {
        *c = *c_ref;
      }
      c->ofs = old_ofs;
    }

    switch(c->type) {
      case t_code:
        c->name = (char *) prim_names[prim_idx_code_start];
        break;

      case t_int:
        asprintf(&c->name, "%lld", (long long) arg1);
        break;

      case t_string:
      case t_word:
      case t_ref:
      case t_get:
      case t_set:
        c->name = calloc(1, c->value.p_len + 1);
        memcpy(c->name, c->value.p, c->value.p_len);
        break;

      case t_prim:
        if(arg1 < dict_size) {
          c->name = dict[arg1].name;
        }
        else {
          fprintf(stderr, "error: word %u not in dictionary\n", (unsigned) arg1);
          return 1;
        }
        break;

      case t_bool:
        asprintf(&c->name, "%s", arg1 ? "true" : "false");
        break;

      case t_nil:
        c->name = strdup("nil");
        break;

      case t_comment:
        if(
          c->value.p &&
          c->value.p_len == 7 &&
          decode_number(c->value.p, 7) == GFXBOOT_MAGIC
        ) {
          if(!c->name) c->name = "# gfxboot magic";
        }
        break;

      case t_xref:
        break;

      default:
        fprintf(stderr, "error: type %d not recognized\n", c->type);
        return 2;
    }
  }

  return 0;
}
