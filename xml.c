// xml.c - deals with custom dungeon XML files

#include <libxml/xmlreader.h>

#define STARTING_DELAY(c) (((c) >= WINDMAGE_1 && (c) <= WINDMAGE_3) || \
		((c) >= LICH_1 && (c) <= LICH_3) || (c) == HARPY || \
		((c) >= SARCO_1 && (c) <= SARCO_3))

// Returns the numeric value of a named attribute of the current node.
// If the attribute is absent, it defaults to 0.
static i8 xml_attr(xmlTextReaderPtr xml, char* attr)
{
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	i32 result = value ? atoi(value) : 0;
	free(value);
	return (i8) result;
}

// Converts a single XML node into an appropriate object (Trap, Tile or Monster).
static void xml_process_node(xmlTextReaderPtr xml, i64 level)
{
	static const Coords trap_dirs[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
	};
	static const i8 wall_hp[] = {1, 1, 5, 0, 4, 4, 0, 2, 3, 5, 4, 0};

	static u64 trap_count = 0;
	static i64 level_count = 0;

	const char *name = (const char*) xmlTextReaderConstName(xml);
	u8 type = (u8) xml_attr(xml, "type");
	i64 subtype = xml_attr(xml, "subtype");
	Coords pos = {xml_attr(xml, "x"), xml_attr(xml, "y")};

	pos += spawn;
	type = type == 255 ? RED_DRAGON : type;

	if (!strcmp(name, "level"))
		++level_count;
	else if (level_count != level)
		return;

	if (!strcmp(name, "trap")) {
		Trap *trap = &traps[trap_count++];
		trap->class = subtype == 8 ? OMNIBOUNCE : type;
		trap->pos = pos;
		trap->dir = trap_dirs[subtype & 7];
	}

	else if (!strcmp(name, "tile")) {
		TILE(pos).class = type >= 100 ? WALL : type < 2 ? FLOOR : type,
		TILE(pos).hp = type >= 100 ? wall_hp[type - 100] : 0;
		TILE(pos).torch = (u8) xml_attr(xml, "torch");
		TILE(pos).zone = xml_attr(xml, "zone");
		if (type == STAIRS)
			stairs = pos;
		if (TILE(pos).torch)
			adjust_lights(pos, +1);
	}

	else if (!strcmp(name, "enemy")) {
		monster_new(type, pos)->delay = STARTING_DELAY(type);
	}

	else if (!strcmp(name, "crate")) {
		Monster *crate = &monsters[monster_count++];
		crate->class = CRATE_1;
		crate->pos = pos;
		crate->hp = 1;
	}
}

// Compares the priorities of two monsters. Callback for qsort.
static i32 compare_priorities(const void *a, const void *b)
{
	u32 pa = CLASS((const Monster*) a).priority;
	u32 pb = CLASS((const Monster*) b).priority;
	return (pb > pa) - (pb < pa);
}

// Initializes the game’s state based on the given custom dungeon file.
// Aborts if the file doesn’t exist or isn’t valid XML.
// Valid, non-dungeon XML yields undefined results (most likely, an empty dungeon).
static void xml_parse(char *file, i64 level)
{
	LIBXML_TEST_VERSION;
	xmlTextReaderPtr xml = xmlReaderForFile(file, NULL, 0);

	while (xmlTextReaderRead(xml) == 1)
		if (xmlTextReaderNodeType(xml) == 1)
			xml_process_node(xml, level);

	if (xmlTextReaderRead(xml) < 0)
		FATAL("Invalid XML file: %s", file);
	xmlFreeTextReader(xml);

	move(&player, spawn);
	qsort(monsters, monster_count, sizeof(*monsters), compare_priorities);
	for (Monster *m = monsters; m->hp; ++m) {
		TILE(m->pos).monster = m;
		(m == monsters ? &player : m - 1)->next = m;
	}
}
