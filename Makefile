CC=gcc
CFLAGS=-g -I. $(OPTFLAGS)
LIBS=$(OPTLIBS)
SOURCES=$(wildcard AltaironAVR/**/*.c AltaironAVR/*.c)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

%.o: %.c $(SOURCES)
	$(CC) -c -o $@ $< $(CFLAGS)

altair8800: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) -lwiringPi

clean:
	rm $(OBJECTS) altair8800
