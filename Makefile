CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-unknown-warning-option
CFLAGS += -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro

SOURCES = $(wildcard *.c) $(wildcard *.h)

.PHONY: all report
all: play solve

play: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"ui.c"' -O0 -g -fsanitize=address,leak,undefined -o $@ main.c

solve: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"route.c"' -O3 -fno-omit-frame-pointer -funroll-loops -o $@ main.c

test: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"test.c"' -O0 -g -fsanitize=address,leak,undefined -o $@ main.c

debug: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"route.c"' -O1 -g -fsanitize=address,leak,undefined -o $@ main.c

report: solve
	perf record -g ./$< LUNGEBARD.xml
	perf report
