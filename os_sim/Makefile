export PATH := D:/gcc/ucrt64/bin:$(PATH)

CC = gcc
CFLAGS = -Wall -Wextra -g -I.

SRCS = main.c sim.c pcb.c memory.c queue.c parser.c syscalls.c mutex.c scheduler.c swap.c gui.c
OBJS = $(SRCS:.c=.o)
TARGET = os_sim.exe

# Raylib flags (static linking)
RAYLIB_FLAGS = -L. -lraylib -lopengl32 -lgdi32 -lwinmm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(RAYLIB_FLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) swap_pid*.txt

.PHONY: all clean
