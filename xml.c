#include <libxml/xmlreader.h>

static int xml_attr(xmlTextReaderPtr xml, char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = atoi(value);
	free(value);
	return result;
}

static Entity xml_tile(uint8_t type) {
	switch (type) {
		case 0:   return (Entity) {.class = FLOOR};
		case 3:   return (Entity) {.class = FLOOR};
		case 4:   return (Entity) {.class = WATER};
		case 8:   return (Entity) {.class = TAR};
		case 9:   return (Entity) {.class = STAIRS};
		case 10:  return (Entity) {.class = FIRE};
		case 11:  return (Entity) {.class = ICE};
		case 17:  return (Entity) {.class = OOZE};
		case 100: return (Entity) {.class = WALL, .hp = 1};
		case 101: return (Entity) {.class = WALL, .hp = 1};
		case 102: return (Entity) {.class = WALL, .hp = 5};
		case 103: return (Entity) {.class = WALL, .hp = 0};
		case 104: return (Entity) {.class = WALL, .hp = 4};
		case 105: return (Entity) {.class = WALL, .hp = 4};
		case 107: return (Entity) {.class = WALL, .hp = 2};
		case 108: return (Entity) {.class = WALL, .hp = 3};
		case 109: return (Entity) {.class = WALL, .hp = 5};
		case 110: return (Entity) {.class = WALL, .hp = 4};
		case 111: return (Entity) {.class = WALL, .hp = 0}; // TODO: metal doors
		default: printf("Unknown tile type: %d\n", type); exit(1);
	}
}

static void xml_process_node(xmlTextReaderPtr xml) {
	const char *name = (const char*) xmlTextReaderConstName(xml);
	if (strcmp(name, "trap") && strcmp(name, "tile") && strcmp(name, "enemy"))
		return;
	uint8_t type = (uint8_t) xml_attr(xml, "type");
	int8_t y = (int8_t) xml_attr(xml, "y") + SPAWN_Y;
	int8_t x = (int8_t) xml_attr(xml, "x") + SPAWN_X;
	if (!strcmp(name, "trap"))
		spawn(TRAP, y, x);
	else if (!strcmp(name, "tile"))
		board[y][x] = xml_tile(type);
	else
		spawn(type, y, x);
}

static void xml_parse(char *file) {
	LIBXML_TEST_VERSION;
	xmlTextReaderPtr xml = xmlReaderForFile(file, NULL, 0);
	if (xml == NULL)
		exit(1);
	while (xmlTextReaderRead(xml) == 1)
		if (xmlTextReaderNodeType(xml) == 1)
			xml_process_node(xml);
	xmlFreeTextReader(xml);
	spawn(PLAYER, SPAWN_Y, SPAWN_X);
}
