CC ?= cc
NAME := sop-backup

CPPFLAGS := -Isrc
CFLAGS ?= -std=c17 -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-unused-const-variable -g -O0
LDFLAGS ?=

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)
DEPFILES := $(OBJECTS:.o=.d)

ifeq ($(CI),1)
CFLAGS += -Werror
endif

ifeq ($(SANITIZERS),1)
CFLAGS += -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif

.PHONY: all clean test

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

src/%.o: src/%.c src/common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

test: $(NAME)
	./comprehensive_test.sh

clean:
	$(RM) $(NAME) $(OBJECTS) $(DEPFILES)

-include $(DEPFILES)