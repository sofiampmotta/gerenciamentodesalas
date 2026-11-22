CC = gcc
CLANG_FORMAT = clang-format
TARGET = bin/programa
C_FILES = $(wildcard src/*.c gui/*.c tests/*.c)

CFLAGS = -Wall -Wextra -Iinclude -Igui $(shell pkg-config --cflags gtk4)
LDLIBS = $(shell pkg-config --libs gtk4)

SRC_GUI = $(wildcard gui/*.c)
SRC_CORE = $(filter-out src/main.c,$(wildcard src/*.c))
OBJ_GUI = $(SRC_GUI:gui/%.c=obj/gui/%.o)
OBJ_CORE = $(SRC_CORE:src/%.c=obj/src/%.o)
OBJ = $(OBJ_GUI) $(OBJ_CORE)

$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) -o $@ $^ $(LDLIBS)

obj/gui/%.o: gui/%.c
	@mkdir -p obj/gui
	$(CC) $(CFLAGS) -c $< -o $@

obj/src/%.o: src/%.c
	@mkdir -p obj/src
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin


# ---------------------------------------------------------------- Testes
# Testes do modulo de persistencia (nao precisa do GTK).
# Rodam dentro de build/ para nao sujar a raiz do projeto.
TEST_BIN = build/test_persistencia

$(TEST_BIN): src/persistencia.c tests/test_persistencia.c include/persistencia.h include/salas.h
	@mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Iinclude src/persistencia.c tests/test_persistencia.c -o $@

test: $(TEST_BIN)
	cd build && ./test_persistencia

format:
	$(CLANG_FORMAT) --style=LLVM -i $(C_FILES)

.PHONY: clean test format
