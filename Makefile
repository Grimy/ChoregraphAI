MAKEFLAGS += --no-builtin-rules -j4

CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-unknown-warning-option
CFLAGS += -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
play test: CFLAGS += -g -fsanitize=address,leak,undefined
solve: CFLAGS += -DJOBS=4 -O3 -lpthread

.PHONY: all report stat
all: play solve test

%: %.c main.c monsters.c xml.c los.gen *.h Makefile
	clang $(CFLAGS) -o $@ $<

solve-perf: solve.c main.c monsters.c xml.c los.gen *.h Makefile
	clang $(CFLAGS) -DJOBS=1 -O3 -lpthread -fno-omit-frame-pointer -fno-inline -o $@ $<

los.gen: los
	./$< >$@

stat: solve-perf
	perf stat ./$< LUNGEBARD.xml

report: solve-perf
	perf record -g ./$< LUNGEBARD.xml
	perf report --no-children
