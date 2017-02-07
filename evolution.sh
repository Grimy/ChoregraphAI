#!/bin/sh

rm los.c

for h in $(git log --reverse --format=%h master); do
	clear
	git checkout "$h"
	perl -nE '/^\w.*?(\w+)(?<!__)\((?=.*[){]$)/?$-=print$1:/^}$/?say": $-":++$-' ./*.c |
	sort -rnk2 |
	sed 55q
	sleep 0.1
done
