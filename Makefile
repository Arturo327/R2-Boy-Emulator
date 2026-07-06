CC = gcc
CFLAGS = -Isrc -Wall -Wextra -O2 -march=native

SRC = $(shell find src -name '*.c')
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))

all: build/r2boy

.PHONY: all test clean clean_sav

build/r2boy: $(OBJ)
	$(CC) $(OBJ) -lSDL2 -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: build/r2boy
	@find tests/mooneye/acceptance/ -name "*.gb" | while read rom; do \
		if [ -f "$$rom" ]; then \
			echo "=== Ejecutando: $$rom ==="; \
			./build/r2boy -d "$$rom"; \
		fi \
	done

clean_sav:
	find tests/ -name "*.sav" -delete

clean:
	rm -f $(OBJ) build/r2boy
