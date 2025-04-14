SAMPLES = $(wildcard sample_?)
SAMPLES_Z = $(addsuffix .z, $(SAMPLES))
SAMPLES_INFL = $(addsuffix .infl, $(SAMPLES))

all: inflate

inflate: inflate.c
	gcc -O2 -Wall $< -o $@

inflate_debug: inflate.c
	gcc -g -O2 -Wall $< -o $@ -DWITH_Z_LOG

zpipe: zpipe.c
	gcc -O2 -Wall $< -o $@ -lz

test: inflate $(SAMPLES_INFL)

sample_%.z: sample_% zpipe
	./zpipe < $< > $@

sample_%.infl: sample_%.z inflate
	./inflate < $< > $@
	cmp --quiet $@ $(basename $@)

clean:
	rm -f inflate inflate_debug zpipe *.o *.z *.infl *~
