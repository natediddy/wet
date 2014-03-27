#
# Makefile
# Nathan Forbes
#

CC = gcc
CFLAGS = -Wall -O2 -g -march=native -mtune=native
LIBS = -lcurl

TARGET = wet

SOURCE = \
	wet.c \
	wet-net.c \
	wet-util.c \
	wet-weather.c

OBJECTS = \
	wet.o \
	wet-net.o \
	wet-util.o \
	wet-weather.o

prefix = /usr/local

ifneq ($(DEBUG),)
	CFLAGS += -DWET_DEBUG
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LIBS)

wet.o: wet.c
	$(CC) $(CFLAGS) -c wet.c

wet-net.o: wet-net.c
	$(CC) $(CFLAGS) -c wet-net.c

wet-util.o: wet-util.c
	$(CC) $(CFLAGS) -c wet-util.c

wet-weather.o: wet-weather.c
	$(CC) $(CFLAGS) -c wet-weather.c

install: $(TARGET)
	install $(TARGET) $(prefix)/bin/$(TARGET)

uninstall:
	rm -f $(prefix)/bin/$(TARGET)

clean:
	rm -rf $(OBJECTS)

clobber:
	rm -rf $(OBJECTS) $(TARGET)

.PHONY: install uninstall clean clobber
