#[no_mangle]

use std::fmt::{Display, Error, Formatter};
use std::ops::{Add, Div, Mul, Sub};

macro_rules! blue   { ($x:tt) => (concat!("\x1b[34m", stringify!($x))); }
macro_rules! green  { ($x:tt) => (concat!("\x1b[92m", stringify!($x))); }
macro_rules! yellow { ($x:tt) => (concat!("\x1b[93m", stringify!($x))); }

const GLYPHS: [&'static str; 4] = [
	".",
	green!(P),
	blue!(P),
	yellow!(P),
];

#[derive(Clone, Copy, Debug)]
#[repr(C)]
struct Coords {
	x: i8,
	y: i8,
}

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

extern "C" {
	fn monster_spawn(class: MonsterClass, pos: Coords, delay: u8) -> *mut Monster;
}

impl Coords {
	fn l1(&self) -> i64 {
		self.x.abs() as i64 + self.y.abs() as i64
	}

	fn l2(&self) -> i64 {
		self.x as i64 * self.x as i64 + self.y as i64 * self.y as i64
	}
}

impl Display for Coords {
	fn fmt(&self, f: &mut Formatter) -> Result<(), Error> {
		write!(f, "({}, {})", self.x, self.y)
	}
}

impl Add for Coords {
	type Output = Coords;
	fn add(self, other: Coords) -> Coords {
		Coords { x: self.x + other.x, y: self.y + other.y }
	}
}

impl Sub for Coords {
	type Output = Coords;
	fn sub(self, other: Coords) -> Coords {
		Coords { x: self.x - other.x, y: self.y - other.y }
	}
}

impl Mul<i8> for Coords {
	type Output = Coords;
	fn mul(self, scalar: i8) -> Coords {
		Coords { x: self.x * scalar, y: self.y * scalar }
	}
}

impl Div<i8> for Coords {
	type Output = Coords;
	fn div(self, scalar: i8) -> Coords {
		Coords { x: self.x / scalar, y: self.y / scalar }
	}
}

fn main() {
	let a = Coords { x: 42, y: 12 };
	println!("{} != {}", (a + a).l2(), (a - a).l1());
	let m = unsafe { monster_spawn(MonsterClass::GreenSlime, a, 3) };
	unsafe {
		println!("{:?}", *m);
	}
	println!("{}, {} and {}", GLYPHS[MonsterClass::GreenSlime as usize],
		GLYPHS[MonsterClass::YellowSlime as usize], GLYPHS[MonsterClass::BlueSlime as usize]);
}
