// xml.c - deals with custom dungeon XML files

#include <libxml/xmlreader.h>

// TODO: tile 106 = ???
// TODO: tile 111 = metal doors

// Returns the numeric value of a named attribute of the current node.
// If the attribute is absent, it defaults to 0.
static int xml_attr(xmlTextReaderPtr xml, char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = value ? atoi(value) : 0;
	free(value);
	return result;
}

// Converts a single XML node into an appropriate object (Trap, Tile or Monster).
static void xml_process_node(xmlTextReaderPtr xml) {
	static uint64_t trap_count = 0;
	const char *name = (const char*) xmlTextReaderConstName(xml);
	uint8_t type = (uint8_t) xml_attr(xml, "type");
	int subtype = xml_attr(xml, "subtype");
	int8_t y = (int8_t) xml_attr(xml, "y") + SPAWN_Y;
	int8_t x = (int8_t) xml_attr(xml, "x") + SPAWN_X;

	if (!strcmp(name, "trap"))
		traps[trap_count++] = (Trap) {
			.class = subtype == 8 ? 0 : type,
			.y = y,
			.x = x,
			.dy = (int8_t[]) {0, 0, 1, -1, 1, 1, -1, -1} [subtype & 7],
			.dx = (int8_t[]) {1, -1, 0, 0, 1, -1, 1, -1} [subtype & 7],
		};

	else if (!strcmp(name, "tile"))
		board[y][x] = (Tile) {
			.class = type >= 100 ? WALL : type < 3 ? FLOOR : type,
			.hp = (int8_t[]) {[100] = 1, 1, 5, 0, 4, 4, -1, 2, 3, 5, 4, 0} [type],
			.torch = (int8_t) xml_attr(xml, "torch"),
			.zone = (int8_t) xml_attr(xml, "zone"),
		};

	else if (!strcmp(name, "enemy"))
		monsters[monster_count++] = (Monster) {
			.class = type,
			.y = y,
			.x = x,
		    .hp = class_infos[type].max_hp,
		};
}

// Initializes the game’s state based on the given custom dungeon file.
// Aborts if the file doesn’t exist or isn’t valid XML.
// Valid, non-dungeon XML yields undefined results (most likely, an empty dungeon).
static void xml_parse(char *file) {
	LIBXML_TEST_VERSION;
	xmlTextReaderPtr xml = xmlReaderForFile(file, NULL, 0);
	while (xmlTextReaderRead(xml) == 1)
		if (xmlTextReaderNodeType(xml) == 1)
			xml_process_node(xml);
	if (xmlTextReaderRead(xml) < 0)
		exit(1);
	xmlFreeTextReader(xml);
	board[SPAWN_Y][SPAWN_X].monster = &player;
}
