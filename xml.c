#include <libxml/xmlreader.h>

static int xml_attr(xmlTextReaderPtr xml, char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = atoi(value);
	free(value);
	return result;
}

static void process_node(xmlTextReaderPtr xml) {
	const char *name = (const char*) xmlTextReaderConstName(xml);
	printf("%s\n", name);
	if (strcmp(name, "trap") && strcmp(name, "tile") && strcmp(name, "enemy"))
		return;
	uint8_t type = (uint8_t) xml_attr(xml, "type");
	uint8_t y = (uint8_t) xml_attr(xml, "y") + SPAWN_Y;
	uint8_t x = (uint8_t) xml_attr(xml, "x") + SPAWN_X;
	if (!strcmp(name, "trap"))
		spawn(TRAP, y, x);
	else if (!strcmp(name, "tile"))
		board[y][x] = type >= 100 ? &dirt_wall : NULL;
	else
		spawn(type, y, x);
}

static void parse_xml(void) {
	LIBXML_TEST_VERSION;
	xmlTextReaderPtr xml = xmlReaderForFile("LUNGEBARD.xml", NULL, 0);
	if (xml == NULL)
		exit(1);
	while (xmlTextReaderRead(xml) == 1)
		if (xmlTextReaderNodeType(xml) == 1)
			process_node(xml);
	xmlFreeTextReader(xml);
}
