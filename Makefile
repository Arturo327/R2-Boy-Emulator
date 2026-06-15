CC = gcc
CFLAGS = -Isrc -g -Wall -Wextra -O2 -march=native

SRC = $(shell find src -name '*.c')
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))

all: build/r2boy

build/r2boy: $(OBJ)
	$(CC) $(OBJ) -lSDL2 -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: build/r2boy
	@for rom in tests/mooneye/*; do \
		if [ -f "$$rom" ]; then \
			echo "=== Ejecutando: $$rom ==="; \
			./build/r2boy -d "$$rom"; \
		fi \
	done

clean_sav:
	find . -name "*.sav" -delete

clean:
	rm -f $(OBJ) build/r2boy
