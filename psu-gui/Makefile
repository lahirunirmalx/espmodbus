# PSU Control GUI - SDL2 Application with multithreading
# Dependencies: libsdl2-dev, libsdl2-ttf-dev

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -pthread
LDFLAGS = -lSDL2 -lSDL2_ttf -lm -pthread

TARGET = psu_gui
SRCS = main.c serial_port.c psu_protocol.c
OBJS = $(SRCS:.c=.o)
SINGLE_TARGET = psu_gui_single
SINGLE_SRCS = main_single.c serial_port.c psu_protocol.c
SINGLE_OBJS = $(SINGLE_SRCS:.c=.o)
TOOLBAR_TARGET = psu_gui_toolbar
TOOLBAR_SRCS = main_toolbar.c serial_port.c psu_protocol.c
TOOLBAR_OBJS = $(TOOLBAR_SRCS:.c=.o)
TOOLBAR_SINGLE_TARGET = psu_gui_toolbar_single
TOOLBAR_SINGLE_SRCS = main_toolbar_single.c serial_port.c psu_protocol.c
TOOLBAR_SINGLE_OBJS = $(TOOLBAR_SINGLE_SRCS:.c=.o)

.PHONY: all clean install

all: $(TARGET) $(SINGLE_TARGET) $(TOOLBAR_TARGET) $(TOOLBAR_SINGLE_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(SINGLE_TARGET): $(SINGLE_OBJS)
	$(CC) $(SINGLE_OBJS) -o $(SINGLE_TARGET) $(LDFLAGS)

$(TOOLBAR_TARGET): $(TOOLBAR_OBJS)
	$(CC) $(TOOLBAR_OBJS) -o $(TOOLBAR_TARGET) $(LDFLAGS)

$(TOOLBAR_SINGLE_TARGET): $(TOOLBAR_SINGLE_OBJS)
	$(CC) $(TOOLBAR_SINGLE_OBJS) -o $(TOOLBAR_SINGLE_TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(SINGLE_OBJS) $(TOOLBAR_OBJS) $(TOOLBAR_SINGLE_OBJS) $(TARGET) $(SINGLE_TARGET) $(TOOLBAR_TARGET) $(TOOLBAR_SINGLE_TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Dependencies
main.o: main.c serial_port.h psu_protocol.h
main_single.o: main_single.c main.c serial_port.h psu_protocol.h
main_toolbar.o: main_toolbar.c serial_port.h psu_protocol.h
main_toolbar_single.o: main_toolbar_single.c serial_port.h psu_protocol.h
serial_port.o: serial_port.c serial_port.h
psu_protocol.o: psu_protocol.c psu_protocol.h serial_port.h
