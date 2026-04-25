CC = gcc
CFLAGS = -Wall -O2 -std=c99
LDFLAGS = -lm

TARGET = ct
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all install uninstall clean