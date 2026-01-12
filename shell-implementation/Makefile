CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = shell.out

.PHONY: all clean run test

all: $(TARGET)
	@echo "Shell compiled successfully!"

$(TARGET): $(OBJECTS) | $(OBJDIR)
	$(CC) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

run: $(TARGET)
	@echo "Starting shell..."
	./$(TARGET)

test: $(TARGET)
	@echo "Running comprehensive tests..."
	@echo "Testing Parts D.1 and D.2..."
	./test_shell.sh
	@echo ""
	@echo "Testing Parts E.3 and E.4..."
	./test_E3_E4.sh

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean run test
