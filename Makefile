CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-unknown-warning-option -Wno-c++-compat -Wno-switch-enum
CFLAGS += -I/usr/include/libxml2 -lxml2
CFLAGS += -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
CFLAGS += -Wno-float-equal -D_GNU_SOURCE -Wno-disabled-macro-expansion

SOURCES = $(wildcard *.c) $(wildcard *.h)

.PHONY: all
all: a.out solve

a.out: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"ui.c"' -O1 -g -fsanitize=address,leak,undefined -o $@ main.c

solve: $(SOURCES) Makefile
	clang $(CFLAGS) -DUI='"genetic.c"' -O3 -funroll-loops -o $@ main.c

# echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# taskset 0x1 ./solve ...
