CC ?= cc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 -Wno-format-truncation
TARGET = isaac-online-modded
SRCS = main.c patch.c eid_patch.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS) patch.h eid_patch.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
