CC = clang
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
CFLAGS += -std=c99 -pedantic -march=native -fstrict-aliasing -fstrict-overflow
CFLAGS += -Weverything -Werror -Wno-unknown-warning-option -Wno-c++-compat -Wno-switch-enum
# CFLAGS += -Ofast -fno-asynchronous-unwind-tables
CFLAGS += -O1 -ggdb -fsanitize=address,leak,undefined
SOURCES = $(wildcard *.c *.h)

a.out: $(SOURCES)
	$(CC) $(CFLAGS) main.c

rtl.expand: $(SOURCES)
	gcc -fdump-rtl-expand=$@ -I/usr/include/libxml2 -S main.c >/dev/null
	rm main.s
