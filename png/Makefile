SAMPLES_PNG = $(wildcard sample_*.png)
SAMPLES_DEBUG = $(addsuffix .debug,$(basename $(SAMPLES_PNG)))

all: png

png: png.c
	gcc -O2 -Wall $< -o $@

png_debug: png.c
	gcc -g -O2 -Wall $< -o $@ -DWITH_Z_LOG -DWITH_PNG_LOG

test: png $(SAMPLES_DEBUG)

sample_%.debug: sample_%.png
	convert $< $@

clean:
	rm -f png png_debug *.o *.debug *~
