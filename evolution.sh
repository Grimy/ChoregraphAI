#!/bin/sh

plotcmd='set yrange[0:1000]; set datafile missing "0"; set xlabel "Commit #"; set ylabel "Lines of code"; plot'

for list in 'main.c cotton.c utils.c' 'play.c ui.c' 'solve.c route.c genetic.c' 'xml.c' 'chore.h cotton.h'; do
	file="${list%% *}"
	plotcmd="$plotcmd '$file' smooth mcsplines,"
	rm "output/$file"
	for h in $(git log --reverse --format=%h master); do
		for rename in $list; do
			git show "$h:$rename" 2>/dev/null
		done | grep -Pc '^\s*+[^/]' >>"output/$file"
	done
done

cd output || exit
gnuplot -p -e "$plotcmd"
