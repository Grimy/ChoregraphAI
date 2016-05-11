CC = clang
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-documentation
CFLAGS += -std=c99 -pedantic -march=native -fstrict-aliasing -fstrict-overflow
CFLAGS += -Weverything -Werror -Wno-c++-compat
# CFLAGS += -Ofast -fno-asynchronous-unwind-tables
CFLAGS += -O1 -ggdb -fsanitize=address,leak,undefined

cotton: cotton.c monsters.c ui.c xml.c
	$(CC) $(CFLAGS) -o $@ $<
