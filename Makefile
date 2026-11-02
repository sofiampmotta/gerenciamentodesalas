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


# ---------------------------------------------------------------- Testes
# Testes do modulo de persistencia (nao precisa do GTK).
# Rodam dentro de build/ para nao sujar a raiz do projeto.
TEST_BIN = build/test_persistencia

$(TEST_BIN): src/persistencia.c tests/test_persistencia.c include/persistencia.h include/salas.h
	@mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Iinclude src/persistencia.c tests/test_persistencia.c -o $@

test: $(TEST_BIN)
	cd build && ./test_persistencia

.PHONY: clean test
