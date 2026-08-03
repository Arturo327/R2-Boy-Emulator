CC = gcc

INC_FLAGS = -Isrc
WARN_FLAGS = -Wall -Wextra
ARCH_FLAGS ?= -march=native
CFLAGS ?= $(INC_FLAGS) $(WARN_FLAGS) -O2 $(ARCH_FLAGS) -pthread
LDLIBS ?= -lSDL2 -lSDL2_ttf -pthread -ljpeg

BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/r2boy

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all test test_mbc test_cgb clean clean_sav

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

debug:
	$(MAKE) all \
		BUILD_DIR=build-debug \
		TARGET=build-debug/r2boy-debug \
		ARCH_FLAGS= \
		CFLAGS="$(INC_FLAGS) $(WARN_FLAGS) -O0 -g3 -pthread -fno-omit-frame-pointer -fsanitize=address,undefined -DDEBUG" \
		LDLIBS="-lSDL2 -lSDL2_ttf -pthread -ljpeg -fsanitize=address,undefined"


test: $(TARGET)
	@find tests/mooneye/acceptance/ -name "*.gb" | while read rom; do \
		./build/r2boy --model DMG -d "$$rom" | grep -e "PASS" -e "FAIL"; \
	done
	@find tests/mooneye/acceptance/ -name "*.sav" -delete

test_cgb: $(TARGET)
	@find tests/mooneye/cgb/ -name "*.gb" | while read rom; do \
		./build/r2boy --model CGB -d "$$rom" | grep -e "PASS" -e "FAIL"; \
	done
	@find tests/mooneye/acceptance/ -name "*.sav" -delete

test_mbc: $(TARGET)
	@find tests/mooneye/emulator-only/ -name "*.gb" | while read rom; do \
		./build/r2boy -d "$$rom" | grep -e "PASS" -e "FAIL"; \
	done
	@find tests/mooneye/emulator-only/ -name "*.sav" -delete

clean_sav:
	find tests/ -name "*.sav" -delete

clean:
	rm -rf build/ build-debug/
