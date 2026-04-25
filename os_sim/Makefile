CC = gcc
CFLAGS = -Wall -Wextra -g -I.

SRCS = main.c sim.c pcb.c memory.c queue.c parser.c syscalls.c mutex.c scheduler.c swap.c
OBJS = $(SRCS:.c=.o)
TARGET = os_sim.exe
GUI_TARGET = os_sim_gui.exe
GUI_SRCS = $(SRCS) gui.c

RAYLIB_FLAGS = -L. -lraylib -lopengl32 -lgdi32 -lwinmm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -DNO_GUI -o $@ $^

gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(RAYLIB_FLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -DNO_GUI -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(GUI_TARGET) swap_pid*.txt

.PHONY: all gui clean
