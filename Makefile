CC = clang
CFLAGS += -I/usr/include/libxml2 -lxml2 -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro
CFLAGS += -std=c99 -pedantic -march=native -fstrict-aliasing -fstrict-overflow
CFLAGS += -Weverything -Werror -Wno-unknown-warning-option -Wno-c++-compat -Wno-shadow
# CFLAGS += -Ofast -fno-asynchronous-unwind-tables
CFLAGS += -O1 -ggdb -fsanitize=address,leak,undefined

a.out: cotton.c monsters.c los.c ui.c xml.c
	$(CC) $(CFLAGS) $<

rtl.expand: cotton
	gcc -fdump-rtl-expand=$@ -I/usr/include/libxml2 -S cotton.c >/dev/null
	rm cotton.s

graph: rtl.expand
	egypt --omit ent_add,ent_rm,ent_move,can_move $< | dot -Tpng | feh -FZ -
