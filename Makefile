CC = gcc
CFLAGS = -Wall -Wextra -O2
SRCS = main.c server.c client.c
OBJS = $(SRCS:.c=.o)
TARGET = chat

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
