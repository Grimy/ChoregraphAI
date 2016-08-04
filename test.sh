#!/bin/sh

set -euo pipefail

solve() {
	bin/solve "BARDZ$1.xml" "$2" |
	grep -Pq " $3\t" || echo "$1-$2" fail
}

make
{
	solve 2 1 '→↑↑↑→→→↑↑'
	solve 2 2 '(↑←↑↑←←←←↑↑↑↑→←|↓←←←←↓↑↑←←↑↑↑↓)'
	solve 2 3 '↓↓←←→→→←↓↓↓'
	solve 3 1 '→→→↑↑↑z↑↑↑'
	solve 3 2 '↑→↑→↓→↓↓→[→↓]→[←→]↑↓↓←←'
	solve 3 3 '↓↓←→→↓↓↓↓'
	solve 4 1 '←↓↓↓↓←→↑→→↓→↑↑'
	solve 4 2 '←←←↓↓↓←←↓↓→↓←↑↑'
	solve 4 3 '↑↑→↓↑↑↑←←←←→→↑←'
} 2>&1 | perl -pe '$|=1;END{$t/=time-$^T;print"routes/s: $t"}$t+=$_'
