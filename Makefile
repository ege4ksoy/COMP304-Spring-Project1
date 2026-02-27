CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = shellish
SRCS = shellish-skeleton.c chatroom.c my_cut.c process_tree.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
