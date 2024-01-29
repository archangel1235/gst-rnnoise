PLUGIN_NAME = gstrnnoise
CC = gcc
CFLAGS = -Wall -O2 -fPIC $(shell pkg-config --cflags gstreamer-1.0)
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-audio-1.0 rnnoise)

# save compiled files in a separate directory build
# create the directory if it does not exist

OBJDIR = build
$(shell mkdir -p $(OBJDIR))

all: $(OBJDIR)/$(PLUGIN_NAME).so

$(OBJDIR)/$(PLUGIN_NAME).so: src/gstrnnoise.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

clean:
	rm -f $(OBJDIR)/$(PLUGIN_NAME).so

