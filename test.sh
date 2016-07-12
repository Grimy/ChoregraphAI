#!/bin/sh

make solve-perf
./solve-perf BARDZ2.xml 1 | grep -q →↑↑↑→→→↑↑         || echo '2-1 fail'
./solve-perf BARDZ2.xml 2 | grep -q ↑←↑↑←←←←↑↑↑↑→←    || echo '2-2 fail'
./solve-perf BARDZ2.xml 3 | grep -q ↓↓←←→→→←↓↓↓       || echo '2-3 fail'
./solve-perf BARDZ3.xml 1 | grep -q →→→↑↑↑z↑↑↑        || echo '3-1 fail'
./solve-perf BARDZ3.xml 2 | grep -Pq '↑→↑→↓→↓↓→[→↓]→[←→]↑↓↓←←' || echo '3-2 fail'
./solve-perf BARDZ3.xml 3 | grep -q ↓↓←→→↓↓↓↓         || echo '3-3 fail'
./solve-perf BARDZ4.xml 1 | grep -q ←↓↓↓↓←→↑→→↓→↑↑    || echo '4-1 fail'
./solve-perf BARDZ4.xml 2 | grep -q ←←←↓↓↓←←↓↓→↓←↑↑   || echo '4-2 fail'
./solve-perf BARDZ4.xml 3 | grep -q ↑↑→↓↑↑↑←←←←→→↑←   || echo '4-3 fail'
