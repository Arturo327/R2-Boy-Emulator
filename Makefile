CC = gcc
CFLAGS = -Isrc -Wall -Wextra -O2 -march=native -pthread
LDLIBS = -lSDL2 -lSDL2_ttf -pthread -ljpeg

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all test clean clean_sav

all: build/r2boy

build/r2boy: $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(LDLIBS) -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

test: build/r2boy
	@find tests/mooneye/acceptance/ -name "*.gb" | while read rom; do \
		./build/r2boy -d "$$rom" | grep -e "PASS" -e "FAIL"; \
	done
	@find tests/mooneye/acceptance/ -name "*.sav" -delete

test_mbc: build/r2boy
	@find tests/mooneye/emulator-only/ -name "*.gb" | while read rom; do \
		./build/r2boy -d "$$rom" | grep -e "PASS" -e "FAIL"; \
	done
	@find tests/mooneye/emulator-only/ -name "*.sav" -delete

clean_sav:
	find tests/ -name "*.sav" -delete

clean:
	rm -r build/
