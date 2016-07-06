MAKEFLAGS += --no-builtin-rules -j4

CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-unknown-warning-option
CFLAGS += -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
play test: CFLAGS += -g -fsanitize=address,leak,undefined
solve: CFLAGS += -DJOBS=4 -O3 -lpthread

.PHONY: all report
all: play solve test

%: %.c main.c monsters.c xml.c *.h Makefile
	clang $(CFLAGS) -o $@ $<

report: CFLAGS += -fno-omit-frame-pointer -fno-inline
report: solve
	perf record -g ./$< LUNGEBARD.xml
	perf report --no-children
