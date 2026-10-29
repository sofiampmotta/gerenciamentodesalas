CC = gcc
TARGET = bin/programa

CFLAGS = -Wall -Wextra -Iinclude $(shell pkg-config --cflags gtk4)
LDLIBS = $(shell pkg-config --libs gtk4)

SRC = $(wildcard src/*.c) 
OBJ = $(SRC:src/%.c=obj/%.o)

$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) -o $@ $^ $(LDLIBS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin/programa

