#include <libxml/xmlreader.h>

static int xml_attr(xmlTextReaderPtr xml, char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = atoi(value);
	free(value);
	return result;
}

static Entity xml_tile(uint8_t type) {
	if (type >= 100)
		return (Entity) {.class = WALL, .hp = 1};
	else if (type == 17)
		return (Entity) {.class = OOZE};
	return (Entity) {.class = FLOOR};
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
