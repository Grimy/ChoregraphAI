MAKEFLAGS += --no-builtin-rules --no-builtin-variables --quiet
OBJECTS = main.o monsters.o xml.o los.o
ARGS := BARDZ4.xml 3

CC = clang++
CFLAGS += -std=c++11 -Weverything -Werror -march=native -mtune=native
CFLAGS += -Wno-old-style-cast -Wno-c99-extensions -Wno-c++98-compat-pedantic -Wno-c++11-narrowing
CFLAGS += -Wno-gnu-statement-expression -Wno-gnu-case-range -Wno-format-nonliteral -Wno-disabled-macro-expansion
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -fno-exceptions -fno-rtti -fopenmp=libomp
CFLAGS += -I/usr/include/libxml2 -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
LDFLAGS += -lxml2 -lm

.PHONY: all debug report stat long-funcs long-lines callgraph
.SECONDARY:

all:
	make -j dbin/play dbin/solve

los.c: los.pl
	./$< >$@

define BUILDTYPE
$(1)/%: CFLAGS += $(2)
$(1)/%.o: %.c chore.h base.h Makefile
	+echo CC $$@
	$(CC) -xc++ -nostdinc++ $$(CFLAGS) $$< -c -o $$@
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

long-funcs:
	perl -nE '/^\w.*?(\w+)\(/?$$-=print$$1:/^}$$/?say": $$-":++$$-' *.c | sort -rnk2 | sed 31q

long-lines:
	perl -CADS -ple '/^if/||s|^|y///c+7*y/\t//." "|e' *.c | sort -rn | sed 31q

callgraph:
	echo digraph { "$$(perl -nle '$$"="|";/^\w.*?(\w+)\(/?push@f,$$f=$$1:/\b(@f)\(/&&print"$$f->$$1"' main.c)" } | dot -Tpng | feh -FZ -

# This was used to generate the priorities. Don’t try this at home.
# perl -pi.bak -e "$(perl -ne'\@_{/^\t\[(?:.*,){5}\s*(\d+)/}}{printf"s/^\\t\\[(?:.*,){6}\\K\\s*$_\\b/%4d/;",$-++for sort{$a<=>$b}keys%_' monsters.c)" monsters.c
