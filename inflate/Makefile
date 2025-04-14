SAMPLES = $(wildcard sample_?)
SAMPLES_Z = $(addsuffix .z, $(SAMPLES))
SAMPLES_INFL = $(addsuffix .infl, $(SAMPLES))

CC = gcc
CFLAGS = -O2 -Wall -Wno-pointer-sign -Wsign-conversion -Wsign-compare

all: inflate

inflate: inflate.c
	$(CC) $(CFLAGS) $< -o $@

inflate_debug: inflate.c
	$(CC) -g $(CFLAGS) $< -o $@ -DWITH_Z_LOG

zpipe: zpipe.c
	$(CC) $(CFLAGS) $< -o $@ -lz

test: inflate $(SAMPLES_Z) $(SAMPLES_INFL)

sample_%.z: sample_% zpipe
	./zpipe < $< > $@

sample_%.infl: sample_%.z inflate
	./inflate < $< > $@
	cmp --quiet $@ $(basename $@)

clean:
	rm -f inflate inflate_debug zpipe *.o *.z *.infl *~
