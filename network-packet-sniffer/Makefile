CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpcap
TARGET = cshark
SRC = cshark.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
