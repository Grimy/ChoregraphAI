// xml.c - deals with custom dungeon XML files

#include "chore.h"

#include <libxml/xmlreader.h>

// Pointer to the end of the monsters array
static Trap *last_trap;

// Returns the numeric value of a named attribute of the current node.
// If the attribute is absent, it defaults to 0.
static i32 xml_attr(xmlTextReader *xml, char* attr)
{
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	i32 result = value ? atoi(value) : 0;
	free(value);
	return result;
}

static void xml_first_pass(xmlTextReader *xml)
{
	spawn.x = max(spawn.x, 1 - (i8) xml_attr(xml, "x"));
	spawn.y = max(spawn.y, 1 - (i8) xml_attr(xml, "y"));
}

static ItemClass xml_item(xmlTextReader *xml, char* attr)
{
	static const char* item_names[ITEM_LAST] = {
		[BOMBS]      = "bomb",
		[BOMBS_3]    = "bomb_3",
		[HEART_1]    = "misc_heart_container",
		[HEART_2]    = "misc_heart_container2",
		[JEWELED]    = "weapon_dagger_jeweled",
		[LUNGING]    = "feet_boots_lunging",
		[MEMERS_CAP] = "head_miners_cap",
		[PACEMAKER]  = "heart_transplant",
	};

	char* item_name = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) attr);

	ItemClass class = ITEM_LAST;
	while (--class && !streq(item_name, item_names[class]));
	free(item_name);
	return class;
}

// Converts a single XML node into an appropriate object (Trap, Tile or Monster).
static void xml_process_node(xmlTextReader *xml)
{
	static const Coords trap_dirs[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}
	};
	static const i8 wall_hp[] = {1, 1, 5, 0, 4, 4, 0, 2, 3, 5, 4, 0};
	static const u8 enemy_id[] = {
		GREEN_SLIME,   // Z1
		SKELETANK_1,   // Z2
		FIRE_SLIME,    // Z3
		BOMBER,        // Z4
		DIREBAT_1,     // Minibosses
		SKELETON_1,    // Debug
		SHOPKEEPER,    // Special
		CRYSTAL_1,     // Z5
	};

	const char *name = (const char*) xmlTextReaderConstName(xml);
	i32 type = xml_attr(xml, "type");
	Coords pos = {(i8) xml_attr(xml, "x"), (i8) xml_attr(xml, "y")};

	pos += spawn;
	if (pos.x >= ARRAY_SIZE(g.board) - 1 || pos.y >= ARRAY_SIZE(*g.board) - 1)
		return;

	if (streq(name, "trap")) {
		i32 subtype = xml_attr(xml, "subtype");
		if (type == 10) {
			monster_spawn(FIREPIG, pos, 0)->dir.x = subtype ? -1 : 1;
			return;
		}
		last_trap->class = subtype == 8 ? OMNIBOUNCE : (u8) type;
		last_trap->pos = pos;
		last_trap->dir = trap_dirs[subtype & 7];
		++last_trap;
	}

	else if (streq(name, "tile")) {
		if (type == 103 || type == 111 || type == 118) {
			type = DOOR;
		} else if (type >= 100) {
			i8 hp = TILE(pos).hp = wall_hp[type - 100];
			i32 zone = xml_attr(xml, "zone");
			g.locking_enemies = 1 + (zone == 4);
			if (zone == 4 && (hp == 1 || hp == 2))
				type = Z4WALL;
			else if (zone == 2 && hp == 2)
				type = FIREWALL;
			else if (zone == 3 && hp == 2)
				type = ICEWALL;
			else
				type = WALL;
		}
		TILE(pos).wired = type == 20 || type == 118;
		TILE(pos).class = (u8) type;
		TILE(pos).torch = (u8) xml_attr(xml, "torch");
		if (type == STAIRS)
			stairs = pos;
		if (TILE(pos).torch)
			adjust_lights(pos, +1, 0);
	}

	else if (streq(name, "enemy")) {
		u8 id = enemy_id[type / 100] + type % 100;
		if (id == GHAST || id == GHOUL || id == WRAITH || id == WIGHT)
			return;
		Monster *m = monster_spawn(id, pos, 0);
		m->aggro = false;
		if (id == RED_DRAGON || id == BLUE_DRAGON)
			m->exhausted = 4;
		else if (id == LIGHTSHROOM)
			adjust_lights(pos, +1, 3);
		else if (id == ZOMBIE || id == WIRE_ZOMBIE)
			m->dir.x = 1;
		else if (m->class == NIGHTMARE_1 || m->class == NIGHTMARE_2)
			g.nightmare = g.last_monster;
	}

	else if (streq(name, "chest")) {
		monster_spawn(CHEST, pos, 0)->item = xml_item(xml, "contents");
	}

	else if (streq(name, "crate")) {
		monster_spawn(CRATE_2 + (u8) type, pos, 0)->item = xml_item(xml, "contents");
	}

	else if (streq(name, "item")) {
		if (L1(pos - spawn))
			TILE(pos).item = xml_item(xml, "type");
		else
			pickup_item(xml_item(xml, "type"));
	}
}

static void xml_process_file(char *file, i64 level, void callback(xmlTextReader *xml))
{
	xmlTextReader *xml = xmlReaderForFile(file, NULL, 0);

	while (xmlTextReaderRead(xml) == 1) {
		if (xmlTextReaderNodeType(xml) != 1)
			continue;
		level -= streq((const char*) xmlTextReaderConstName(xml), "level");
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
void xml_parse(i32 argc, char **argv)
{
	if (argc < 2)
		FATAL("Usage: %s dungeon_file.xml [level]", argv[0]);
	i32 level = argc == 3 ? *argv[2] - '0' : 1;

	last_trap = g.traps;

	LIBXML_TEST_VERSION;
	xml_process_file(argv[1], level, xml_first_pass);
	monster_spawn(PLAYER, spawn, 0);
	xml_process_file(argv[1], level, xml_process_node);

	assert(player.class == PLAYER);

	update_fov();
}
