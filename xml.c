// xml.c - deals with custom dungeon XML files

#include <libxml/xmlreader.h>

// Returns the numeric value of a named attribute of the current node.
// If the attribute is absent, it defaults to 0.
static i8 xml_attr(xmlTextReader *xml, char* attr)
{
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	i32 result = value ? atoi(value) : 0;
	free(value);
	return (i8) result;
}

static void xml_first_pass(xmlTextReader *xml)
{
	spawn.x = max(spawn.x, 4 - xml_attr(xml, "x"));
	spawn.y = max(spawn.y, 4 - xml_attr(xml, "y"));
}

// Converts a single XML node into an appropriate object (Trap, Tile or Monster).
static void xml_process_node(xmlTextReader *xml)
{
	static const Coords trap_dirs[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}
	};
	static const i8 wall_hp[] = {1, 1, 5, 0, 4, 4, 0, 2, 3, 5, 4, 0};
	static u64 trap_count = 0;

	const char *name = (const char*) xmlTextReaderConstName(xml);
	u8 type = (u8) xml_attr(xml, "type");
	i64 subtype = xml_attr(xml, "subtype");
	Coords pos = {xml_attr(xml, "x"), xml_attr(xml, "y")};

	pos += spawn;
	assert(max(pos.x, pos.y) <= ARRAY_SIZE(g.board) - 4);
	type = type == 255 ? MOMMY : type;

	if (!strcmp(name, "trap")) {
		if (type == 10) {
			last_monster->state = !subtype;
			monster_init(last_monster++, FIREPIG, pos);
			return;
		}
		Trap *trap = &g.traps[trap_count++];
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
		monster_init(last_monster++, type, pos);
		if ((type >= SARCO_1 && type <= SARCO_3) || type == MOMMY)
			last_monster++->class = type;
	}

	else if (!strcmp(name, "crate")) {
		monster_init(last_monster++, CRATE_2 + type, pos);
	}
}

// Compares the priorities of two monsters. Callback for qsort.
static i32 compare_priorities(const void *a, const void *b)
{
	u32 pa = CLASS((const Monster*) a).priority;
	u32 pb = CLASS((const Monster*) b).priority;
	return (pb > pa) - (pb < pa);
}

static void xml_process_file(char *file, i64 level, void callback(xmlTextReader *xml)) {
	xmlTextReader *xml = xmlReaderForFile(file, NULL, 0);

	while (xmlTextReaderRead(xml) == 1) {
		if (xmlTextReaderNodeType(xml) != 1)
			continue;
		level -= !strcmp((const char*) xmlTextReaderConstName(xml), "level");
		if (!level)
			callback(xml);
	}

	if (xmlTextReaderRead(xml) < 0)
		FATAL("Invalid XML file: %s", file);
	xmlFreeTextReader(xml);
}

// Initializes the game’s state based on the given custom dungeon file.
// Aborts if the file doesn’t exist or isn’t valid XML.
// Valid, non-dungeon XML yields undefined results (most likely, an empty dungeon).
static void xml_parse(char *file, i64 level)
{
	LIBXML_TEST_VERSION;
	xml_process_file(file, level, xml_first_pass);
	xml_process_file(file, level, xml_process_node);

	move(&player, spawn);
	for (i64 i = 0; i < 5; ++i)
		*last_monster++ = (Monster) {.class = BOMB, .aggro = true};

	qsort(g.monsters, (size_t) (last_monster - g.monsters),
			sizeof(*g.monsters), compare_priorities);
	for (Monster *m = g.monsters; m < last_monster; ++m) {
		TILE(m->pos).monster = m;
		if (m->class == NIGHTMARE_1 || m->class == NIGHTMARE_2)
			nightmare = m;
	}
}
