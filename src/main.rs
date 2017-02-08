mod coords;

use coords::Coords;
use std::process::Command;
use std::io::{Read, stdin};

#[no_mangle]
extern "C" {
	fn xml_parse(argc: i32, argv: *mut *mut i8);
	fn do_beat(input: u8) -> bool;
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
	lord: bool,
	untrapped: bool,
	electrified: bool,
	knocked: bool,
	requeued: bool,
	was_requeued: bool,
}

fn main() {
	let a = Coords { x: 42, y: 12 };
	println!("{} != {}", (a + a).l2(), (a - a).l1());
	println!("{}, {}, {} and {}",
		GLYPHS[MonsterClass::None as usize],
		GLYPHS[MonsterClass::GreenSlime as usize],
		GLYPHS[MonsterClass::YellowSlime as usize],
		GLYPHS[MonsterClass::BlueSlime as usize]);

	unsafe {
		xml_parse(2, 0 as *mut *mut i8);
	}

	Command::new("stty").args(&["-echo", "-icanon", "eol", "\x01"]).spawn().expect("fail");
	let mut buffer = [0;1];

	loop {
		stdin().read(&mut buffer).unwrap();
		match buffer[0] {
			4 | 27 | b'q' => { break }
			x => unsafe { do_beat(x); }
		}
	}
}
