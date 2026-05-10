CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -std=c11 -Iinclude
LDFLAGS := -lreadline

SRC_DIR := src
OBJ_DIR := build
BIN     := mysh

SRCS    := $(wildcard $(SRC_DIR)/*.c)
OBJS    := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all debug clean test install

all: CFLAGS += -O2
all: $(BIN)

debug: CFLAGS += -g3 -DDEBUG -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Built: $(BIN)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN)

install: all
	cp $(BIN) /usr/local/bin/$(BIN)
	@echo "Installed to /usr/local/bin/$(BIN)"

test: all
	@echo "Running tests..."
	@bash tests/run_tests.sh
