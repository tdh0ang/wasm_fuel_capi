CC = gcc
CFLAGS = -Wall -Wextra -g \
  -I/home/tdhoang/Documents/01_IT/01_Coding/sched_a653lib/wasmtime-v34.0.1-x86_64-linux-c-api/include
LDFLAGS = -L/home/tdhoang/Documents/01_IT/01_Coding/sched_a653lib/wasmtime-v34.0.1-x86_64-linux-c-api/lib -lwasmtime

WAT_FILE = main.wat
WASM_FILE = main.wasm

SRCS = wasm_api.c
OBJS = $(SRCS:.c=.o)
TARGET = sched

.PHONY: all clean

all: $(WASM_FILE) $(TARGET)

$(WASM_FILE): $(WAT_FILE)
	wat2wasm $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@rm -f $(OBJS)

clean:
	rm -f $(TARGET) $(WASM_FILE)

