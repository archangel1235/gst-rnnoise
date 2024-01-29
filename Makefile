PLUGIN_NAME = gstrnnoise
CC = gcc
CFLAGS = -Wall -O2 -fPIC $(shell pkg-config --cflags gstreamer-1.0)
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-audio-1.0 rnnoise)

all: $(PLUGIN_NAME).so

$(PLUGIN_NAME).so: gstrnnoise.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PLUGIN_NAME).so

