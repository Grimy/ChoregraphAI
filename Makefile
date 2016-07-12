MAKEFLAGS += --no-builtin-rules -j4

CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression -Wno-gnu-case-range
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-unknown-warning-option
CFLAGS += -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
play test solve-dbg: CFLAGS += -g -fsanitize=address,leak,undefined
solve-dbg:  CFLAGS += -DJOBS=4 -lpthread
solve:      CFLAGS += -DJOBS=4 -O3 -lpthread
solve-perf: CFLAGS += -DJOBS=1 -O3 -lpthread -fno-omit-frame-pointer -fno-inline

ARGS := BARDZ4.xml 3

.PHONY: all report stat debug
all: play solve test

%: %.c main.c monsters.c xml.c los.c *.h Makefile
	clang $(CFLAGS) -o $@ $<

solve-%: solve.c main.c monsters.c xml.c los.c *.h Makefile
	clang $(CFLAGS) -o $@ $<

los.c: los.pl
	./$< >$@

debug: solve-dbg
	lldb ./$< $(ARGS)

stat: solve-perf
	perf stat ./$< $(ARGS)

report: solve-perf
	perf record -g ./$< $(ARGS)
	perf report --no-children
