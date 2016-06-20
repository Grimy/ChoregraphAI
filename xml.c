// xml.c - deals with custom dungeon XML files

#include <string.h>
#include <libxml/xmlreader.h>

// Returns the numeric value of a named attribute of the current node.
// If the attribute is absent, it defaults to 0.
static i8 xml_attr(xmlTextReaderPtr xml, char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	i32 result = value ? atoi(value) : 0;
	free(value);
	return (i8) result;
}

// Converts a single XML node into an appropriate object (Trap, Tile or Monster).
static void xml_process_node(xmlTextReaderPtr xml) {
	static const Coords trap_dirs[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
	};
	static const i8 wall_hp[] = {1, 1, 5, 0, 4, 4, 0, 2, 3, 5, 4, 0};

	static u64 trap_count = 0;
	const char *name = (const char*) xmlTextReaderConstName(xml);
	u8 type = (u8) xml_attr(xml, "type");
	i64 subtype = xml_attr(xml, "subtype");
	Coords pos = {xml_attr(xml, "x"), xml_attr(xml, "y")};

	pos += spawn;
	type = type == 255 ? SKELETON_3 : type;

	if (!strcmp(name, "trap"))
		traps[trap_count++] = (Trap) {
			.class = subtype == 8 ? OMNIBOUNCE : type,
			.pos = pos,
			.dir = trap_dirs[subtype & 7],
		};

	else if (!strcmp(name, "tile")) {
		TILE(pos) = (Tile) {
			.class = type >= 100 ? WALL : type < 2 ? FLOOR : type,
			.hp = type >= 100 ? wall_hp[type - 100] : 0,
			.torch = xml_attr(xml, "torch"),
			.zone = xml_attr(xml, "zone"),
		};
		if (type == STAIRS)
			stairs = pos;
	}

	else if (!strcmp(name, "enemy"))
		monsters[monster_count++] = (Monster) {
			.class = type,
			.pos = pos,
			.prev_pos = pos,
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
		FATAL("Invalid XML file: %s", file);
	xmlFreeTextReader(xml);
	move(&player, spawn);
}
