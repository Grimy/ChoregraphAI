#!perl -l

use List::Util qw(min max);

@ok = (
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
@light = (102, 102, 102, -1, 102, 102, -1, -1, 102,
	94, 83, -1, -1, 53, -1, -1, 19, 10, 2);

# 4.2494 to 4.2505 would do
my $a = 4.25;

sub lit {
	my $light = 0;
	$light += $a - sqrt for @$_;
	# $light += $light[$_] for @$_;
	return $light;
}

print min map lit, @ok;
print max map lit, @ko;
