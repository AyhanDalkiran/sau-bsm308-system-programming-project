# Directories
SRC_DIR := src
INC_DIR := inc
OBJ_DIR := obj
BIN_DIR := bin
TARGET := $(BIN_DIR)/tarsau

# Compiler and flags
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -pedantic-errors

# Source and object files
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

## Targets
all: clean | $(TARGET)

# OBJ and BIN directories cleanup
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*

# Executable
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

.PHONY: all clean