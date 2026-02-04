# Compiler
CC = gcc

# Common flags
CFLAGS = -Wall -g

# Detect OS
UNAME_S := $(shell uname -s)

# OS-specific flags for OpenSSL (libcrypto)
ifeq ($(UNAME_S),Linux)
    LIBS = -lcrypto
endif
ifeq ($(UNAME_S),Darwin) # Darwin is the kernel name for macOS
    BREW_PREFIX := $(shell brew --prefix openssl)
    INCLUDES = -I$(BREW_PREFIX)/include
    LDFLAGS = -L$(BREW_PREFIX)/lib
    LIBS = -lcrypto
endif

# Target executables
TARGETS = server client

# Default rule
all: $(TARGETS)

# Rule to build the server
server: server.c sham.h
	$(CC) $(CFLAGS) server.c -o server $(INCLUDES) $(LDFLAGS) $(LIBS)

# Rule to build the client
client: client.c sham.h
	$(CC) $(CFLAGS) client.c -o client $(INCLUDES) $(LDFLAGS) $(LIBS)

# Clean up build files
clean:
	rm -f $(TARGETS) *.o *~ server_log.txt client_log.txt

.PHONY: all clean