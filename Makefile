CC = gcc
CFLAGS = -Wall -Wextra -g \
  -I/capi/include
LDFLAGS = -L/capi/lib -lwasmtime

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

