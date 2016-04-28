CC=gcc
CFLAGS=-g -Wall -O
SRC_DIR = ./src
SRC_EXT = c
KERNEL = $(shell uname -s)
MACHINE = $(shell uname -m)
DST_DIR = ./bin/$(MACHINE)/$(KERNEL)
DIRECTORIES = $(DST_DIR)

all: $(DIRECTORIES) controller

controller: $(DST_DIR)/controller

$(DST_DIR)/%: $(SRC_DIR)/%.$(SRC_EXT)
	$(CC) $< -o $@ $(CFLAGS)

run: $(DST_DIR)/controller
	$<

.PHONY: directories

directories: $(DIRECTORIES)

clean:
	rm controller
	rm -rf $(DST_DIR)/*

$(DIRECTORIES):
	mkdir -p $(DST_DIR)
