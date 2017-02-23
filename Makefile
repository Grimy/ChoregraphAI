MAKEFLAGS += --no-builtin-rules --no-builtin-variables --quiet
OBJECTS = main.o monsters.o xml.o los.o
ARGS := BARDZ4.xml 3

CFLAGS += -std=c++11 -Weverything -Werror -march=native -mtune=native
CFLAGS += -Wno-c++98-compat-pedantic -Wno-c99-extensions -Wno-c++11-narrowing
CFLAGS += -Wno-old-style-cast -Wno-format-nonliteral -Wno-missing-field-initializers
CFLAGS += -Wno-gnu-designator -Wno-gnu-case-range
CFLAGS += -fstrict-aliasing -fstrict-overflow -fno-asynchronous-unwind-tables
CFLAGS += -fno-exceptions -fno-rtti -fopenmp=libomp

.PHONY: all callgraph debug fuzz long-funcs long-lines report stat
.SECONDARY:

all:
	make -j dbin/play dbin/solve

los.c: los.pl
	./$< >$@

define BUILDTYPE
$(1)/%.o: %.c chore.h Makefile
	+echo CC $$@
	mkdir -p $(1)
	$(2) -xc++ -nostdinc++ $$(CFLAGS) $$< -c -o $$@
$(1)/%: $(1)/%.o $(addprefix $(1)/, $(OBJECTS))
	+echo LD $$@
	$(2) $$(CFLAGS) -lm $$^ -o $$@
endef

$(eval $(call BUILDTYPE, bin, clang++ -g -O3 -flto -fno-omit-frame-pointer))
$(eval $(call BUILDTYPE, dbin, clang++ -g -fsanitize=undefined,thread))
$(eval $(call BUILDTYPE, fbin, afl-clang-fast++ -O3 -flto))

callgraph:
	echo digraph { "$$(perl -nle '$$"="|";/^\w.*?(\w+)\(/?push@f,$$f=$$1:/\b(@f)\(/&&print"$$f->$$1"' main.c)" } | dot -Tpng | feh -FZ -

debug: dbin/solve
	lldb $< $(ARGS)

fuzz: fbin/play
	afl-fuzz -idungeons -ooutput $^ @@ -ibomb -m'eifj<  zz'

long-funcs:
	perl -nE '/^\w.*?(\w+)\(/?$$-=print$$1:/^}$$/?say": $$-":++$$-' *.c | sort -rnk2 | sed 31q

long-lines:
	perl -CADS -ple '/^if/||s|^|y///c+7*y/\t//." "|e' *.c | sort -rn | sed 31q

report: bin/solve
	perf record -e $${EVENT-cycles} -g ./test.sh
	perf report --no-children

stat: bin/solve
	perf stat -d -e{task-clock,page-faults,cycles,instructions,branch,branch-misses} ./test.sh

# This was used to generate the priorities. Don’t try this at home.
# perl -pi.bak -e "$(perl -ne'\@_{/^\t\[(?:.*,){5}\s*(\d+)/}}{printf"s/^\\t\\[(?:.*,){6}\\K\\s*$_\\b/%4d/;",$-++for sort{$a<=>$b}keys%_' monsters.c)" monsters.c
