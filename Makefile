# CCC Makefile
# Some parts from here: https://github.com/mbcrawfo/GenericMakefile

BIN_NAME := ccc
CC ?= cc
DEBUG_FLAGS = -DDEBUG
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -O0
LDFLAGS = -lm

SRC = src
DEST = bin
BUILD_DIR = $(DEST)/build

INC = -I$(SRC)/


# Find all source files in the source directory, sorted by most
# recently modified
SOURCES := $(shell find $(SRC)/ -name '*.c' -printf '%T@\t%p\n' \
					| sort -k 1nr | cut -f2-)
OBJS := $(SOURCES:$(SRC)/%.c=$(BUILD_DIR)/%.o)
# Set the dependency files that will be used to add header dependencies
DEPS := $(OBJS:.o=.d)

all: $(DEST)/$(BIN_NAME)

# Create the directories used in the build
.PHONY: dirs
dirs:
	@mkdir -p $(dir $(OBJS))

$(DEST)/$(BIN_NAME): dirs $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

.PHONY: clean
clean:
	$(RM) -r $(DEST)/*

# Add dependency files, if they exist
-include $(DEPS)

$(BUILD_DIR)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) $(INC) -MP -MMD -c $< -o $@
