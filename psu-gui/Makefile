# PSU Control GUI - SDL2 Application with multithreading
# Dependencies: libsdl2-dev, libsdl2-ttf-dev

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -pthread
LDFLAGS = -lSDL2 -lSDL2_ttf -lm -pthread

TARGET = psu_gui
SRCS = main.c serial_port.c psu_protocol.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Dependencies
main.o: main.c serial_port.h psu_protocol.h
serial_port.o: serial_port.c serial_port.h
psu_protocol.o: psu_protocol.c psu_protocol.h serial_port.h
