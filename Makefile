GIT2LOG := $(shell if [ -x ./git2log ] ; then echo ./git2log --update ; else echo true ; fi)
GITDEPS := $(shell [ -d .git ] && echo .git/HEAD .git/refs/heads .git/refs/tags)
VERSION := $(shell $(GIT2LOG) --version VERSION ; cat VERSION)
BRANCH  := $(shell git branch | perl -ne 'print $$_ if s/^\*\s*//')
PREFIX  := gfxboot2-$(VERSION)

ifdef 32BIT
  OPT_32BIT = -m32
endif

CC      = gcc
CFLAGS  = -g -O2 $(OPT_32BIT) -I. -Wall -Wno-pointer-sign -Wsign-conversion -Wsign-compare
LDFLAGS = $(OPT_32BIT)

GFXBOOT_MAIN       = files/main.gs

GFXBOOT_LIB_SRC    = gfxboot.c gfxboot_main.c \
                     gfxboot_array.c gfxboot_canvas.c gfxboot_draw.c gfxboot_font.c gfxboot_hash.c gfxboot_context.c \
                     gfxboot_lib.c gfxboot_malloc.c gfxboot_jpeg.c \
                     gfxboot_mem.c gfxboot_num.c gfxboot_obj.c gfxboot_olist.c gfxboot_prim.c gfxboot_debug.c
GFXBOOT_LIB_OBJ    = $(GFXBOOT_LIB_SRC:.c=.o)

GFXBOOT_BIN_SRC    = gfxboot-compile.c gfxboot-x11.c
GFXBOOT_BIN_OBJ    = $(GFXBOOT_BIN_SRC:.c=.o)

GFXBOOT_HEADER     = gfxboot.h vocabulary.h

GRUB_MODULE_BIOS = $(shell . ./config_vars ; echo $$grub_module_bios)
GRUB_MODULE_EFI  = $(shell . ./config_vars ; echo $$grub_module_efi)
GRUB_FILES       = $(shell . ./config_vars ; echo $$grub_files)
GRUB_ISO         = $(shell . ./config_vars ; echo $$grub_iso)

ifneq "$(vm)" ""
  VM = --$(vm)
endif

.PHONY: all grub-bios grub-efi grub-iso test-bios test-efi test-x11 archive clean tests

all: gfxboot-x11 gfxboot-compile gfxboot-font

changelog: $(GITDEPS)
	$(GIT2LOG) --changelog changelog

doc: doc/reference.adoc

doc/reference.adoc: vocabulary.def gfxboot_prim.c doc/reference_template
	./mk_reference vocabulary.def gfxboot_prim.c doc/reference_template $@

grub-bios: $(GRUB_MODULE_BIOS)

grub-efi: $(GRUB_MODULE_EFI)

grub-iso: gfxboot-compile $(GRUB_ISO)

test-bios: grub-iso
	vm --cdrom $(GRUB_ISO) $(VM) --serial

test-efi: grub-iso
	vm --cdrom $(GRUB_ISO) $(VM) --serial --efi

test-x11: gfxboot-x11 gfxboot-compile
	./prepare_files $(GFXBOOT_MAIN) x11
	./gfxboot-x11 x11

test-console: gfxboot-x11 gfxboot-compile
	./prepare_files $(GFXBOOT_MAIN) x11
	./gfxboot-x11 --no-x11 --file - x11

vocabulary.h: vocabulary.def types.def
	./mk_vocabulary vocabulary.def types.def $@

$(GFXBOOT_LIB_OBJ): %.o: %.c $(GFXBOOT_HEADER)
	$(CC) -c $(CFLAGS) -ffreestanding $<

$(GFXBOOT_BIN_OBJ): %.o: %.c $(GFXBOOT_HEADER)
	$(CC) -c $(CFLAGS) $<

gfxboot-x11: gfxboot-x11.o $(GFXBOOT_LIB_OBJ)
	$(CC) $< $(GFXBOOT_LIB_OBJ) $(LDFLAGS) -lX11 -o $@

gfxboot-compile: gfxboot-compile.o
	$(CC) $< $(LDFLAGS) -o $@

gfxboot-font: gfxboot-font.c
	$(CC) $(CFLAGS) -I /usr/include/freetype2 -lfreetype $< -o $@

$(GRUB_MODULE_BIOS): $(GRUB_FILES)
	./grub_build --bios

$(GRUB_MODULE_EFI): $(GRUB_FILES)
	./grub_build --efi

$(GRUB_ISO): $(GRUB_MODULE_BIOS) $(GRUB_MODULE_EFI)
	./mk_grub_test

tests: gfxboot-x11 gfxboot-compile
	@./run_tests

archive: changelog
	@if [ ! -d .git ] ; then echo no git repo ; false ; fi
	mkdir -p package
	git archive --prefix=$(PREFIX)/ $(BRANCH) > package/$(PREFIX).tar
	tar -r -f package/$(PREFIX).tar --mode=0664 --owner=root --group=root --mtime="`git show -s --format=%ci`" --transform='s:^:$(PREFIX)/:' VERSION changelog
	xz -f package/$(PREFIX).tar

clean:
	rm -f changelog VERSION vocabulary.h
	rm -f $(GRUB_ISO) screenlog.0 *~ *.o gfxboot-{x11,font,compile} sample *.log files/*~ *.gc doc/*~
	rm -f tests/*~ tests/*/{*.log,*~,*.gc,gc.log.ref,opt*.log.ref}
	rm -rf x11 tmp grub package
