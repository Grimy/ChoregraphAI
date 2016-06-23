# ChoregraphAI — an AI for Crypt of the NecroDancer

## About

ChoregraphAI’s goal is to automically determine the optimal sequence of moves
to complete a NecroDancer level. It takes as input a custom dungeon file, and
outputs the optimal route (as well as various stats).

It achieves this with two components. First, the **simulator**: basically
a complete reimplementation of NecroDancer’s game logic as a terminal app. It
can run either interactively (in full ASCII-art glory) or non-interactively.

The second component is a genetic algorithm. It spins up thousands of
non-interactive instances of the simulator per second, feeds them
randomly-mutated routes, and selects for further investigation those routes
that performed the best on various criteria (getting close to the stairs,
staying alive and killing the miniboss being the main factors).

## History

**2016-06-20** ChoregraphAI found [its first route](https://www.youtube.com/watch?v=dNYfj2hQ3kI&feature=youtu.be)!

## License

This work is licensed under a [Creative Commons Attribution-NonCommercial 4.0
International License](http://creativecommons.org/licenses/by-nc/4.0/).
