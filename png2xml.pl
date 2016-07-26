#!/usr/bin/perl

use GD::Image;
GD::Image->trueColor(1);
$\ = $" = $/; #"

open PNG, "@ARGV" or die;
$png = GD::Image->newFromPng(\*PNG) or die;

my %traps = (
	2  => "type='9'",
	3  => "type='1' subtype='0'",
	4  => "type='1' subtype='8'",
	5  => "type='4'",
	6  => "type='1' subtype='4'",
	7  => "type='10' subtype='0'",
	9  => "type='6'",
	10 => "type='7'",
	11 => "type='2'",
	13 => "type='5'",
	14 => "type='3'",
);

my %tiles = (
	4   => "type='10' zone='2'",
	5   => "type='11' zone='3'",
	7   => "type='17' zone='4'",
	8   => "type='4' zone='0'",
	10  => "type='3'",
	19  => "type='106'",
	22  => "type='111'",
	23  => "type='111'",
	24  => "type='106'",
	26  => "type='0' zone='0'",
	27  => "type='0' zone='0'",
	29  => "type='8' zone='1'",
	47  => "type='9'",
	51  => "type='9'",
	60  => "type='108'",
	61  => "type='108'",
	62  => "type='100' zone='0'",
	63  => "type='100' zone='0'",
	64  => "type='100' zone='0'",
	65  => "type='100' zone='0'",
	66  => "type='100' zone='0'",
	67  => "type='100' zone='1'",
	68  => "type='100' zone='1'",
	69  => "type='100' zone='1'",
	70  => "type='100' zone='1'",
	71  => "type='100' zone='2'",
	72  => "type='100' zone='2'",
	73  => "type='100' zone='2'",
	74  => "type='100' zone='2'",
	75  => "type='100' zone='3'",
	76  => "type='100' zone='3'",
	77  => "type='100' zone='3'",
	78  => "type='100' zone='3'",
	79  => "type='100' zone='4'",
	80  => "type='100' zone='4'",
	81  => "type='100' zone='4'",
	82  => "type='100' zone='4'",
	83  => "type='104'",
	84  => "type='104' cracked='1'",
	85  => "type='104'",
	86  => "type='104' cracked='1'",
	87  => "type='107' zone='0'",
	88  => "type='100' torch='1'",
	89  => "type='100' zone='1'",
	90  => "type='108' cracked='1'",
	91  => "type='100' cracked='1' zone='0'",
	92  => "type='107' cracked='1' zone='0'",
	93  => "type='0' zone='1'",
	94  => "type='0' zone='1'",
	95  => "type='100' cracked='1' zone='1'",
	96  => "type='107' zone='1'",
	97  => "type='107' cracked='1' zone='1'",
	98  => "type='0' zone='2'",
	99  => "type='0' zone='3'",
	100 => "type='100' zone='3'",
	101 => "type='100' cracked='1' zone='3'",
	102 => "type='100' zone='2'",
	103 => "type='100' cracked='1' zone='2'",
	104 => "type='107' zone='3'",
	105 => "type='107' cracked='1' zone='3'",
	106 => "type='107' zone='2'",
	107 => "type='107' cracked='1' zone='2'",
	108 => "type='0' zone='4'",
	109 => "type='108' zone='4'",
	110 => "type='108' cracked='1' zone='4'",
	111 => "type='108' zone='4'",
	112 => "type='100' zone='4'",
	113 => "type='100' cracked='1' zone='4'",
	114 => "type='107' zone='4'",
	115 => "type='107' cracked='1' zone='4'",
);

my %shrines = (
	34 => 0,
	35 => 12,
	36 => 1,
	37 => 2,
	38 => 9,
	39 => 11,
	40 => 3,
	41 => 10,
	42 => 4,
	43 => 5,
	44 => 6,
	45 => 7,
	46 => 8,
);

my %enemies = (
	7   => 110,
	8   => 303,
	9   => 111,
	10  => 100,
	11  => 102,
	12  => 101,
	13  => 149,
	14  => 150,
	15  => "BARREL",
	16  => 6,
	17  => 302,
	18  => 8,
	19  => 401,
	20  => 400,
	21  => 7,
	23  => 209,
	24  => 210,
	28  => 304,
	29  => 305,
	31  => 217,
	32  => 215,
	33  => 218,
	34  => 216,
	53  => "CHEST",
	54  => "CHEST",
	55  => "CHEST",
	56  => "CHEST",
	57  => 112,
	58  => 112,
	59  => 112,
	60  => 112,
	61  => 112,
	62  => 112,
	63  => 112,
	64  => 112,
	65  => 112,
	66  => 112,
	77  => "CRATE",
	83  => 404,
	84  => 402,
	85  => 403,
	87  => 116,
	88  => 205,
	91  => 322,
	92  => 323,
	93  => 324,
	94  => 325,
	95  => 326,
	96  => 327,
	97  => 327,
	98  => 214,
	99  => 11,
	100 => 306,
	101 => 207,
	102 => 208,
	103 => 300,
	104 => 301,
	105 => 109,
	106 => 307,
	107 => 108,
	109 => 52,
	110 => 211,
	111 => 206,
	121 => 309,
	122 => 311,
	123 => 310,
	130 => 407,
	131 => 408,
	132 => 114,
	133 => 114,
	134 => 155,
	135 => 9,
	136 => 313,
	137 => 10,
	138 => 312,
	140 => 321,
	141 => 106,
	142 => 118,
	143 => 117,
	144 => 107,
	147 => 409,
	148 => 410,
	154 => 412,
	155 => 412,
	156 => 412,
	157 => 412,
	160 => 314,
	171 => "POLTERGEIST",
	178 => 315,
	179 => 316,
	180 => 317,
	181 => 600,
	187 => 212,
	188 => 219,
	189 => 3,
	190 => 5,
	191 => 105,
	192 => 103,
	193 => 104,
	194 => 4,
	195 => 202,
	196 => 203,
	197 => 204,
	200 => 1,
	201 => 200,
	202 => 0,
	203 => 201,
	204 => 2,
	205 => 62,
	206 => 113,
	207 => "URN",
	213 => 14,
	214 => 15,
	215 => "WHITE CHEST MIMIC",
	219 => 319,
	220 => 320,
	222 => 115,
	223 => 13,
	224 => 213,
	225 => 12,
);

my @tiles = ();
my @traps = ();
my @enemies = ();
my @items = ();
my @chests = ();
my @crates = ();
my @shrines = ();

sub parse {
	my ($r, $g, $b) = @_;
	if ($r) {
		die "Unknown trap: $r" unless $traps{$r};
		push @traps, "<trap x='$x' y='$y' $traps{$r} />";
	}

	if ($shrines{$g}) {
		push @shrines, "<shrine x='$x' y='$y' type='$shrines{$g}' />";
		push @tiles, "<tile x='$x' y='$y' type='0' />";
	}

	elsif ($tiles{$g}) {
		push @tiles, "<tile x='$x' y='$y' $tiles{$g} />";
	}

	elsif ($g && $g != 25) {
		die "Unknown tile: $g";
	}

	if ($enemies{$b}) {
		push @enemies, "<enemy x='$x' y='$y' $enemies{$b} />";
	}

	elsif ($b) {
		die "Unknown entity: $b";
	}
}

for $y (0..40) {
	for $x (0..75) {
		print 48 + 24*$x, ' ', 72 + 24*$y;
		parse($png->rgb($png->getPixel(48 + 24*$x, 72 + 24*$y)));
	}
}

print "<tiles>\n@tiles\n</tiles>";
print "<traps>\n@traps\n</traps>";
print "<enemies>\n@enemies\n</enemies>";
print "<items>\n@items\n</items>";
print "<chests>\n@chests\n</chests>";
print "<crates>\n@crates\n</crates>";
print "<shrines>\n@shrines\n</shrines>";
