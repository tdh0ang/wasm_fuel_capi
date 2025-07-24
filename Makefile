CC = gcc
CFLAGS = -Wall -Wextra -g -I/capi/include
LDFLAGS = -L/capi/lib -lwasmtime

WAT_FILES := $(wildcard wasm/*.wat)
WASM_FILES := $(WAT_FILES:.wat=.wasm)

SRCS = main.c src/wasm_api.c
OBJS = $(SRCS:.c=.o)
TARGET = sched

.PHONY: all clean

all: $(WASM_FILES) $(TARGET)

wasm/%.wasm: wasm/%.wat
	wat2wasm $< -o $@
	@echo "> Generated $@ from $< successfully."

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "> Compiled $< successfully."

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@if [ $$? -eq 0 ]; then \
		echo "> Linked $@ successfully."; \
		rm -f $(OBJS); \
		echo "> Build complete. Run './$(TARGET)' to execute the program."; \
	else \
		echo "> Linking failed."; \
		exit 1; \
	fi

clean:
	rm -f $(TARGET) $(WASM_FILES)
	@echo "> Cleaning finished!"
