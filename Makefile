CC = gcc
CFLAGS = -g -Wall -std=gnu11 -Wextra -Werror -I./include

SRCDIR = src
INCDIR = include
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)

TARGET = object

.PHONY: all clean

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(SRCDIR)/main.c $(SOURCES) $(INCDIR)/*.h
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(BINDIR)/$(TARGET)