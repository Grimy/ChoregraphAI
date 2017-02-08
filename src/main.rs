#![feature(thread_local)]

mod coords;

use coords::Coords;
use std::process::Command;
use std::io::{Read, stdin};
use std::mem::size_of;

#[no_mangle]
extern "C" {
	fn xml_parse(argc: i32, argv: *mut *mut i8);
	fn do_beat(input: u8) -> bool;
	#[thread_local]
	static g: GameState;
}

macro_rules! blue   { ($x:tt) => (concat!("\x1b[34m", stringify!($x))); }
macro_rules! green  { ($x:tt) => (concat!("\x1b[92m", stringify!($x))); }
macro_rules! yellow { ($x:tt) => (concat!("\x1b[93m", stringify!($x))); }

const GLYPHS: [&'static str; 4] = [
	".",
	green!(P),
	blue!(P),
	yellow!(P),
];

#[derive(Debug)]
#[repr(u8)]
enum MonsterClass {
	None,
	GreenSlime,
	BlueSlime,
	YellowSlime,
}

// A “Monster” can be either an enemy, a bomb, or the player (yes, we are all monsters).
#[derive(Debug)]
#[repr(C)]
struct Monster {
	pos: Coords,
	prev_pos: Coords,
	dir: Coords,
	class: MonsterClass,
	hp: u8,
	delay: u8,
	confusion: u8,
	freeze: u8,
	state: u8,
	exhausted: u8,
	item: u8,
	aggro: bool,
	flags: u8,
}

// Items
#[repr(u8)]
enum ItemType {
	NO_ITEM,
	BOMBS,
	BOMBS_3,
	HEART_1,
	HEART_2,
	JEWELED,
	LUNGING,
	MEMERS_CAP,
	PACEMAKER,
	SCROLL_FREEZE,
	LAST,
}

// Tile types.
#[repr(u8)]
enum TileType {
	FLOOR = 0,
	SHOP_FLOOR = 3,
	WATER = 4,
	TAR = 8,
	STAIRS = 9,
	FIRE = 10,
	ICE = 11,
	OOZE = 17,
	WIRE = 20,
}

#[repr(C)]
struct Tile {
	class: TileType,
	hp: i8,
	monster: u8,
	item: ItemType,
	light: u16,
	revealed: bool,
	flags: u8,
}

// Trap types.
#[derive(Debug)]
#[repr(u32)]
enum TrapType {
	OMNIBOUNCE,
	BOUNCE,      // any of the eight directional bounce traps
	SPIKE,
	TRAPDOOR,
	CONFUSE,
	TELEPORT,
	TEMPO_DOWN,
	TEMPO_UP,
	RAND_BOUNCE, // not implemented
	BOMBTRAP,
}

#[derive(Debug)]
#[repr(C)]
struct Trap {
	class: TrapType,
	pos: Coords,
	dir: Coords,
}

#[repr(C)]
struct GameState {
	// Contents of the level
	board: [[Tile; 32]; 32],
	monsters: [Monster; 72],
	traps: [Trap; 32],

	// Global properties
	seed: u64,
	input: [u8; 32],
	current_beat: u8,
	locking_enemies: u8,
	nightmare: u8,
	monkeyed: u8,
	mommy_spawn: u8,
	sarco_spawn: u8,
	last_monster: u8,

	// Attributes specific to the player
	inventory: [u8; 10],
	player_moved: bool,
	sliding_on_ice: bool,
	boots_on: bool,
	iframes: u8,
}

fn main() {
	let a = Coords { x: 42, y: 12 };
	println!("{} != {}", (a + a).l2(), (a - a).l1());
	// println!("{}, {}, {} and {}",
		// GLYPHS[MonsterClass::None as usize],
		// GLYPHS[MonsterClass::GreenSlime as usize],
		// GLYPHS[MonsterClass::YellowSlime as usize],
		// GLYPHS[MonsterClass::BlueSlime as usize]);

	// println!("sizeof: {}", size_of::<GameState>());
	// unsafe { println!("index0: {:?}", &g.monsters[0] as *const Monster); }
	// unsafe { println!("index1: {:?}", &g.monsters[1] as *const Monster); }

	unsafe {
		xml_parse(2, 0 as *mut *mut i8);
	}

	println!("done parsing");

	Command::new("stty").args(&["-echo", "-icanon", "eol", "\x01"]).spawn().expect("fail");
	let mut buffer = [0;1];

	// unsafe { println!("{:?}", g.monsters[0]); }
	// unsafe { println!("{:?}", g.monsters[1]); }
	// unsafe { println!("{:?}", g.traps[0]); }
	// unsafe { println!("{:?}", g.traps[1]); }

	while unsafe { g.monsters[1].hp > 0 } {
		stdin().read(&mut buffer).unwrap();
		match buffer[0] {
			4 | 27 | b'q' => { break }
			x => unsafe { do_beat(x); }
		}
	}
}
