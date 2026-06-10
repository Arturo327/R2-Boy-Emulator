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

run: build/r2boy
	./build/r2boy

clean:
	rm -f $(OBJ) build/r2boy
