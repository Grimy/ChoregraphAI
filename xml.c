#include <libxml/xmlreader.h>

static xmlTextReaderPtr xml;

static int xml_attr(char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = value ? atoi(value) : 0;
	free(value);
	return result;
}

static void xml_tile(uint8_t type, Tile *tile) {
	// TODO: 106 = ???
	// TODO: 111 = metal doors
	static const int8_t type_to_hp[12] = {1, 1, 5, 0, 4, 4, -1, 2, 3, 5, 4, 0};
	static const TileClass type_to_class[112] = {
		FLOOR, FLOOR, FLOOR, WATER, [8] = TAR, STAIRS, FIRE, ICE, [17] = OOZE,
	};
	tile->class = type_to_class[type];
	if (tile->class == WALL)
		tile->hp = type_to_hp[type - 100];
	tile->zone = (int8_t) xml_attr("zone");
	tile->torch = (int8_t) xml_attr("torch");
}

static void xml_process_node(xmlTextReaderPtr xml) {
	static uint64_t trap_count = 0;
	const char *name = (const char*) xmlTextReaderConstName(xml);

	uint8_t type = (uint8_t) xml_attr("type");
	int8_t y = (int8_t) xml_attr("y") + SPAWN_Y;
	int8_t x = (int8_t) xml_attr("x") + SPAWN_X;

	if (!strcmp(name, "trap"))
		traps[trap_count++] = (Trap) {.class = type, .y = y, .x = x};
	else if (!strcmp(name, "tile"))
		xml_tile(type, &board[y][x]);
	else if (!strcmp(name, "enemy"))
		monsters[monster_count++] = (Monster) {.class = type, .y = y, .x = x};
}

static void xml_parse(char *file) {
	LIBXML_TEST_VERSION;
	xml = xmlReaderForFile(file, NULL, 0);
	if (xml == NULL)
		exit(1);
	while (xmlTextReaderRead(xml) == 1)
		if (xmlTextReaderNodeType(xml) == 1)
			xml_process_node(xml);
	xmlFreeTextReader(xml);
	board[SPAWN_Y][SPAWN_X].next = &player;
}
