#!/bin/sh

set -euo pipefail

solve() {
	printf "%d-%d: " "$1" "$2"
	if out=$({ /bin/time -f%e bin/solve "BARDZ$1.xml" "$2" | grep -Pq " $3\t"; } 2>&1); then
		set -- $out
		echo "$1 beats in $2s"
		time=$(echo "$time + $2" | bc)
		beats=$(echo "$beats + $1" | bc)
	else
		echo "[31mfail[m"
	fi
}

make

time=0
beats=0

solve 2 1 '(→↑↑↑→→→↑↑|↓→→→↑↑↑↑←)'
solve 2 2 '([↑←][↑←]↑↑←←←←↑↑↑↑[z→]←|←←[←↓][←↓]←←↑↑↑↑↓↓→←)'
solve 2 3 '↓↓←←→→→←↓↓↓'
solve 3 1 '→→→↑↑↑z↑↑↑'
solve 3 2 '↑→↑→↓→↓↓→[→↓]→[←→]↑↓↓←←'
solve 3 3 '↓↓←→→↓↓↓↓'
solve 4 1 '←↓↓↓↓←→↑→→↓→↑↑'
solve 4 2 '←←←↓↓↓←←↓↓→↓←↑↑'
solve 4 3 '↑↑→↓↑↑↑←←←←→→↑←'

mbps=$(echo "scale=3; $beats / $time / 1000000" | bc)
echo "$mbps Mbps"
