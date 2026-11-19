# Host tests for the code in flipper-app/core. Run: make -C tests
CORE := ../flipper-app/core
CFLAGS := -std=gnu17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
          -I$(CORE) -I../flipper-app/lib/jsmn
SRCS := $(wildcard $(CORE)/*.c)
TESTS := $(patsubst %.c,build/%,$(wildcard test_*.c))

all: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

build/%: %.c $(SRCS) $(wildcard $(CORE)/*.h) check.h
	@mkdir -p build
	$(CC) $(CFLAGS) $< $(SRCS) -o $@

clean:
	rm -rf build

.PHONY: all clean
