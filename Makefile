MAKEFLAGS += --no-builtin-rules --no-builtin-variables --quiet
OBJECTS = main.o monsters.o xml.o los.o
ARGS := BARDZ4.xml 3

CC = clang
CFLAGS += -std=c99 -Weverything -Werror -march=native -mtune=native
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -Wno-switch -Wno-switch-enum -Wno-gnu-statement-expression -Wno-gnu-case-range
CFLAGS += -I/usr/include/libxml2 -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
LDFLAGS += -lxml2 -lm
%/solve: CFLAGS += -fopenmp=libomp

.PHONY: all debug report stat rust
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

$(eval $(call BUILDTYPE, bin, -g -O3 -fno-omit-frame-pointer))
$(eval $(call BUILDTYPE, dbin, -g -fsanitize=undefined,thread))

debug: dbin/solve
	lldb $< $(ARGS)

report: bin/solve
	perf record -e $${EVENT-cycles} -g ./test.sh
	perf report --no-children

stat: bin/solve
	perf stat -d -e{task-clock,page-faults,cycles,instructions,branch,branch-misses} ./test.sh

long-funcs:
	perl -nE '/^\w.*?(\w+)\(/?$$-=print$$1:/^}$$/?say": $$-":++$$-' *.c | sort -rnk2 | sed 31q

long-lines:
	perl -CADS -ple '/^if/||s|^|y///c+7*y/\t//." "|e' *.c | sort -rn | sed 31q

bin/lib%.a: %.c
	+echo CC $@
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

rust: bin/libmain.a bin/libmonsters.a bin/liblos.a
	cargo build
