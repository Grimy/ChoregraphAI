#include <libxml/xmlreader.h>

static xmlTextReaderPtr xml;

static int xml_attr(char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = value ? atoi(value) : 0;
	free(value);
	return result;
}

static const int8_t subtype_to_dy[] = {0, 0, 1, -1, 1, 1, -1, -1, 0};
static const int8_t subtype_to_dx[] = {1, -1, 0, 0, 1, -1, 1, -1, 0};
static const int8_t type_to_hp[12] = {1, 1, 5, 0, 4, 4, -1, 2, 3, 5, 4, 0};
static const TileClass type_to_class[112] = {
	FLOOR, FLOOR, FLOOR, WATER, [8] = TAR, STAIRS, FIRE, ICE, [17] = OOZE,
};

// TODO: 106 = ???
// TODO: 111 = metal doors

static void xml_process_node(xmlTextReaderPtr xml) {
	static uint64_t trap_count = 0;
	const char *name = (const char*) xmlTextReaderConstName(xml);

	uint8_t type = (uint8_t) xml_attr("type");
	int8_t y = (int8_t) xml_attr("y") + SPAWN_Y;
	int8_t x = (int8_t) xml_attr("x") + SPAWN_X;

	if (!strcmp(name, "trap")) {
		Trap *trap = &traps[trap_count++];
		int subtype = xml_attr("subtype");
		trap->y = y;
		trap->x = x;
		trap->class = subtype == 8 ? 0 : type;
		trap->dy = subtype_to_dy[subtype];
		trap->dx = subtype_to_dx[subtype];
	}
	else if (!strcmp(name, "tile")) {
		Tile *tile = &board[y][x];
		tile->class = type_to_class[type];
		tile->hp = tile->class == WALL ? type_to_hp[type - 100] : 0;
		tile->zone = (int8_t) xml_attr("zone");
		tile->torch = (int8_t) xml_attr("torch");
	}
	else if (!strcmp(name, "enemy")) {
		Monster *m = &monsters[monster_count++];
		m->class = type;
		m->y = y;
		m->x = x;
		m->hp = CLASS(m).max_hp;
	}
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
