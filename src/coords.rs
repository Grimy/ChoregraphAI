use std::fmt::{Display, Error, Formatter};
use std::ops::{Add, Div, Mul, Sub};

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct Coords {
	pub x: i8,
	pub y: i8,
}

impl Coords {
	pub fn l1(&self) -> i64 {
		self.x.abs() as i64 + self.y.abs() as i64
	}

	pub fn l2(&self) -> i64 {
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
