CC = clang
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
CFLAGS += -std=c99 -pedantic -march=native -fstrict-aliasing -fstrict-overflow
CFLAGS += -Weverything -Werror -Wno-unknown-warning-option -Wno-c++-compat -Wno-shadow
# CFLAGS += -Ofast -fno-asynchronous-unwind-tables
CFLAGS += -O1 -ggdb -fsanitize=address,leak,undefined
SOURCES = $(wildcard *.c *.h)

a.out: $(SOURCES)
	$(CC) $(CFLAGS) main.c

rtl.expand: $(SOURCES)
	gcc -fdump-rtl-expand=$@ -I/usr/include/libxml2 -S main.c >/dev/null
	rm main.s

graph: rtl.expand
	egypt --omit ent_add,ent_rm,ent_move,can_move $< | dot -Tpng | feh -FZ -
