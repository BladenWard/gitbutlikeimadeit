TARGET := gblimi

CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c99 -pedantic -O3

INCLUDE := -lssl -lcrypto -lz

OBJ_DIR := obj
BIN_DIR := bin

SRC := $(wildcard *.c)
OBJ := $(SRC:%.c=$(OBJ_DIR)/%.o)

$(BIN_DIR)/$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -r $(BIN_DIR) $(OBJ_DIR)
