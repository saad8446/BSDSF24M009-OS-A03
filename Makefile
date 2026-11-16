CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
LDFLAGS =

SRC    = $(wildcard src/*.c)
OBJ    = $(SRC:src/%.c=obj/%.o)
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ) | bin
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

obj/%.o: src/%.c include/shell.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

bin obj:
	mkdir -p $@

clean:
	rm -rf obj bin

.PHONY: all clean