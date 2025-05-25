# Variables
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g
TARGET = aesdsocket
SRC = aesdsocket.c
OBJ = $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Build the target application
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)

# Phony targets
.PHONY: all clean
