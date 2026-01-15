CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude -Idep

MINGW = x86_64-w64-mingw32-gcc

SRCS = src/main.c src/utils.c src/des.c src/netntlmv1.c src/rainbow.c src/table.c src/lookup.c src/opencl_host.c src/opencl_dyn.c
TARGET = gpu_lookup

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -ldl

windows: $(SRCS)
	$(MINGW) $(CFLAGS) $(SRCS) -o $(TARGET).exe

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean windows