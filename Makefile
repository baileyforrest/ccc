# CCC Makefile
# Some parts from here: https://github.com/mbcrawfo/GenericMakefile

BIN_NAME := ccc
CC ?= cc
DEBUG_FLAGS= -DDEBUG
CFLAGS= -std=c11 -Wall -Wextra -g -O0

SRC= src
DEST= bin
BUILD_DIR= $(DEST)/build

INC= -I$(SRC)


# Find all source files in the source directory, sorted by most
# recently modified
SOURCES = $(shell find $(SRC)/ -name '*.c' -printf '%T@\t%p\n' \
					| sort -k 1nr | cut -f2-)
OBJS= $(SOURCES:$(SRC)/%.c=$(BUILD_DIR)/%.o)

all: $(DEST)/$(BIN_NAME)

# Create the directories used in the build
.PHONY: dirs
dirs:
	@mkdir -p $(dir $(OBJS))

$(DEST)/$(BIN_NAME): dirs $(OBJS)
	$(CC) $(OBJS) -o $@

.PHONY: clean
clean:
	$(RM) -r $(DEST)/*

$(BUILD_DIR)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
