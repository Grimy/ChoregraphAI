CC = clang
CFLAGS += -std=c99 -pedantic -march=native -fstrict-aliasing -fstrict-overflow
CFLAGS += -Weverything -Werror -Wno-c++-compat
# CFLAGS += -Ofast -fno-asynchronous-unwind-tables
CFLAGS += -O1 -ggdb -fsanitize=address,leak,undefined

.PHONY: run
run: cotton
	./$< $(ARGS)

.PHONY: debug
debug: $(NAME)-debug
	gdb -q -ex 'br main' -ex 'r $(ARGS)' ./$<
