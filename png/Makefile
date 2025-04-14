SAMPLES_PNG = $(wildcard sample_*.png)
SAMPLES_DEBUG = $(addsuffix .debug,$(basename $(SAMPLES_PNG)))
SAMPLES_DECODE = $(addsuffix .decode,$(basename $(SAMPLES_PNG)))

CC = gcc
CFLAGS = -O2 -Wall -Wno-pointer-sign -Wsign-conversion -Wsign-compare

all: png

png: png.c
	$(CC) $(CFLAGS) $< -o $@

png_debug: png.c
	$(CC) -g $(CFLAGS) $< -o $@ -DWITH_PNG_LOG

test: png $(SAMPLES_DECODE) $(SAMPLES_DEBUG)

sample_%.debug: sample_%.png
	convert $< $@

sample_%.decode: sample_%.png sample_%.debug png
	./png < $< > $@ 2> $(@)_2
	cmp --quiet $(basename $@).debug $@

clean:
	rm -f png png_debug *.o *.debug *.decode *.decode_2 *~
