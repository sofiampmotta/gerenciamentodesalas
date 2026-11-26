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
# Testes de persistencia e exportacao.
# Rodam dentro de build/ para nao sujar a raiz do projeto.
TEST_BIN = build/test_persistencia
TEST_EXPORT_BIN = build/test_exportacao
TEST_LEITOR_BIN = build/test_leitor_pdf

$(TEST_BIN): src/persistencia.c src/alocacao.c src/aplicacao.c tests/test_persistencia.c include/persistencia.h include/salas.h include/alocacao.h include/aplicacao.h
	@mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Iinclude src/persistencia.c src/alocacao.c src/aplicacao.c tests/test_persistencia.c -o $@

$(TEST_EXPORT_BIN): src/exportacao.c tests/test_exportacao.c include/exportacao.h
	@mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Iinclude $(shell pkg-config --cflags gtk4) src/exportacao.c tests/test_exportacao.c -o $@ $(shell pkg-config --libs gtk4)

$(TEST_LEITOR_BIN): src/leitor_pdf.c src/persistencia.c src/alocacao.c src/aplicacao.c tests/test_leitor_pdf.c include/leitor_pdf.h include/persistencia.h include/aplicacao.h
	@mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Iinclude src/leitor_pdf.c src/persistencia.c src/alocacao.c src/aplicacao.c tests/test_leitor_pdf.c -o $@

test: $(TEST_BIN) $(TEST_EXPORT_BIN) $(TEST_LEITOR_BIN)
	cd build && ./test_persistencia && ./test_exportacao && ./test_leitor_pdf

format:
	$(CLANG_FORMAT) --style=LLVM -i $(C_FILES)

.PHONY: clean test format
