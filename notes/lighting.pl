#!perl -l

use List::Util qw(min max);

@ok = (
	[5],
	[9, 18, 18, 18, 18],
	[10, 16],
	[13, 13],
	[13, 16, 16, 16],
	[13, 16, 16, 17, 18],
	[13, 16, 17, 17, 17],
	[(16) x 4,  (17) x 2,  (18) x 3],
	[(16) x 3,  (17) x 4,  (18) x 3],
	[(16) x 2,  (17) x 6,  (18) x 2],
	[(16) x 1,  (17) x 8,  (18) x 2],
);

@ko = (
	[9, 18, 18, 18],
	[10, 17, 18, 18, 18, 18],
	[13, 16],
	[13, 16, 16, 17],
	[13, 16, 17, 17, 18],
	[(16) x 4,  (17) x 2,  (18) x 2],
	[(16) x 3,  (17) x 4,  (18) x 2],
	[(16) x 2,  (17) x 6,  (18) x 1],
	[(16) x 1,  (17) x 8,  (18) x 1],
);

# Simplified: 9 8 5 2 1
my @light = (102, 102, 102, -1, 102, 102, -1, -1, 102,
	94, 83, -1, -1, 53, -1, -1, 19, 10, 2);

# wall torch: 4.25
# light mushroom: 4.5
# base torch (held): ?
# bright torch (held): ?
# luminous torch (held): ?
# base torch (ground): ?
# bright torch (ground): ?
# luminous torch (ground): ?

sub lit {
	my $light = 0;
	$light += int($a * (4.25 - sqrt)) for @$_;
	# $light += $light[$_] for @$_;
	return $light;
}

while ($a < 3000) {
	$a += 4;
	my $min = (min map lit, @ok);
	my $max = (max map lit, @ko);
	print "Yay $a: $min > $max!" if $min > $max * 1.002;
}
