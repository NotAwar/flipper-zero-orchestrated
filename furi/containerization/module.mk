SRC_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

SRC += $(wildcard $(SRC_DIR)/*.c)

CFLAGS += -I$(SRC_DIR)
