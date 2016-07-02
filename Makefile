CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-unknown-warning-option
CFLAGS += -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
DEBUG = -O0 -g -fsanitize=address,leak,undefined
OPTI = -O3 -fno-omit-frame-pointer -funroll-loops

SOURCES = $(wildcard *.c) $(wildcard *.h)

.PHONY: all report
all: play solve

play: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"ui.c"' $(DEBUG) -o $@ main.c

solve: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"route.c"' $(OPTI) -o $@ main.c

test: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"test.c"' $(DEBUG) -o $@ main.c

debug: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"route.c"' $(DEBUG) -o $@ main.c

report: solve
	perf record -g ./$< LUNGEBARD.xml
	perf report
