#!/bin/sh

set -euo pipefail

solve() {
	printf "%d-%d: " "$1" "$2"
	if out=$({ /bin/time -f%e bin/solve -l "$2" "dungeons/BARDZ$1.xml" | grep -Pq "\t$3$"; } 2>&1); then
		set -- $out
		echo "$1 beats in $2s"
		time=$(echo "$time + $2" | bc)
		beats=$(echo "$beats + $1" | bc)
	else
		echo "[31mfail[m"
	fi
}

make -j bin/solve

time=0
beats=0

solve 2 1 '((ij|ji)jjiiijj|fiiijjjje)'
solve 2 2 '((je|ej)jjeeeejjjj[ i]e|ee(eef|efe|fee)ejjjjffie)'
solve 2 3 'ffeeiiiefff'
solve 3 1 'iiijjj jjj'
solve 3 2 'jijififfi(fii|iie)jffee'
solve 3 3 'ffei(ei|if)fff'
solve 4 1 'ff((fff|eff|ejj)eeeee|(ffe|eji)eijii)(fijj|iifi)'
solve 4 2 'feeffffefeeefej'
solve 4 3 'jjifjjjeeeeiije'
solve 5 1 'iifffeifffj'
solve 5 2 'ifififfffff'
solve 5 3 'jjfijjiieejii'

mbps=$(echo "scale=3; $beats / $time / 1000000" | bc)
echo "$beats beats in ${time}s ($mbps Mbps)"
