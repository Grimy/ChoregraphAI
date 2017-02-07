MAKEFLAGS += --no-builtin-rules --no-builtin-variables --quiet
OBJECTS = main.o monsters.o xml.o los.o
ARGS := BARDZ4.xml 3

CC = clang
CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-c++-compat -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression -Wno-gnu-case-range -Wno-disabled-macro-expansion
CFLAGS += -I/usr/include/libxml2 -Wno-unknown-warning-option -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
LDFLAGS += -lxml2 -lm
%/solve: CFLAGS += -fopenmp=libomp

.PHONY: all debug report stat
.SECONDARY:

all:
	make -j dbin/play dbin/solve

los.c: los.pl
	./$< >$@

define BUILDTYPE
$(1)/%: CFLAGS += $(2)
$(1)/%.o: %.c chore.h base.h Makefile
	+echo CC $$@
	$(CC) $$(CFLAGS) $$< -c -o $$@
$(1)/%: $(1)/%.o $(addprefix $(1)/, $(OBJECTS))
	+echo LD $$@
	$(CC) $$(CFLAGS) $(LDFLAGS) $$^ -o $$@
endef

$(eval $(call BUILDTYPE, bin, -g -O3 -flto -fno-omit-frame-pointer))
$(eval $(call BUILDTYPE, dbin, -g -fsanitize=undefined,thread))

debug: dbin/solve
	lldb $< $(ARGS)

report: bin/solve
	perf record -e $${EVENT-cycles} -g ./test.sh
	perf report --no-children

stat: bin/solve
	perf stat -d -e{task-clock,page-faults,cycles,instructions,branch,branch-misses} ./test.sh
