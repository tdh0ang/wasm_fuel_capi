CC = gcc
CFLAGS = -Wall -Wextra -g \
  -I/capi/include
LDFLAGS = -L/capi/lib -lwasmtime

# WAT_FILE = wasm/fib.wat
# WASM_FILE = wasm/fib.wasm

WAT_FILES := $(wildcard wasm/*.wat)
WASM_FILES := $(WAT_FILES:.wat=.wasm)

SRCS = src/wasm_api.c
OBJS = $(SRCS:.c=.o)
TARGET = sched

.PHONY: all clean

all: $(WASM_FILES) $(TARGET)

wasm/%.wasm: wasm/%.wat
	wat2wasm $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@rm -f $(OBJS)

clean:
	rm -f $(TARGET) $(WASM_FILES)

