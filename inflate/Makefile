SAMPLES = $(wildcard sample_?)
SAMPLES_Z = $(addsuffix .z, $(SAMPLES))

all: inflate

inflate: inflate.c
	gcc -O2 -Wall $< -o $@

inflate_debug: inflate.c
	gcc -g -O2 -Wall $< -o $@ -DWITH_Z_LOG

zpipe: zpipe.c
	gcc -O2 -Wall $< -o $@ -lz

test: inflate zpipe $(SAMPLES_Z)

sample_%.z: sample_%
	./zpipe < $< > $@
	./inflate < $@ > $(<).infl
	cmp --quiet $(<).infl $<

clean:
	rm -f inflate inflate_debug zpipe *.o *.z *.infl *~
