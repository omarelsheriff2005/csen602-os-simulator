CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS = main.c pcb.c memory.c queue.c parser.c syscalls.c mutex.c scheduler.c swap.c sim.c
OBJS = $(SRCS:.c=.o)
TARGET = os_sim

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -DNO_GUI -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -DNO_GUI -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) swap_pid*.txt

.PHONY: all clean
