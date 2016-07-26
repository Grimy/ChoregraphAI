MAKEFLAGS += --no-builtin-rules -j4
CC = clang
OBJECTS = main.o monsters.o xml.o los.o
ARGS := BARDZ4.xml 3

CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression -Wno-gnu-case-range
CFLAGS += -I/usr/include/libxml2 -Wno-unknown-warning-option -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
LDFLAGS += -lxml2
%/solve: LDFLAGS += -lpthread

.PHONY: all debug report stat
all: dbin/play bin/solve

los.c: los.pl
	./$< >$@

bin/%: CFLAGS += -O3 -fno-omit-frame-pointer -fno-inline
bin/%.o: %.c chore.h base.h Makefile
	$(CC) $(CFLAGS) $< -c -o $@
bin/%: bin/%.o $(addprefix bin/, $(OBJECTS))
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

dbin/%: CFLAGS += -g -fsanitize=undefined,thread
dbin/%.o: %.c chore.h base.h Makefile
	$(CC) $(CFLAGS) $< -c -o $@
dbin/%: dbin/%.o $(addprefix dbin/, $(OBJECTS))
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

debug: solve-dbg
	lldb ./$< $(ARGS)

report: solve-perf
	perf record -g ./$< $(ARGS)
	perf report --no-children

stat: solve-perf
	perf stat ./$< $(ARGS)
