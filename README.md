# ChoregraphAI — an AI for Crypt of the NecroDancer

## About

ChoregraphAI’s purpose is to automically determine the optimal “route”
(sequence of inputs) to complete a NecroDancer level. It takes as input a
custom dungeon file, and outputs several suggested routes, along with some
stats about them.

ChoregraphAI essentially performs a brute-force search over all possible routes
for the level. It prunes out routes which kill the character, or which don’t
get close enough to the stairs after some time. This still leaves millions of
routes to examine, which could not be done in a reasonable time with an
input/output layer on top of NecroDancer.

This is why ChoregraphAI uses its own heavily optimized, pure C
re-implementation of (the relevant parts of) NecroDancer. It can simulate
millions of beats per second, with bug-for-bug accuracy.

Since NecroDancer isn’t open-source, this was achieved by black-box
reverse-engineering: set up edge-case scenarios in the game, see what happens,
then find patterns. This extensive testing led to the discovery and report of
several bugs in NecroDancer; some of them were later fixed.

The simulation can also be run interactively, in full ASCII-art glory, for
debugging or entertainment purposes.

## History

* **2017-02-05** Finished updating ChoregraphAI to [handle DLC enemies](https://www.youtube.com/watch?v=3bm50q3bK8o&feature=youtu.be).

* **2017-01-24** The Amplified DLC for NecroDancer was released, adding many new enemies and game mechanics to simulate.

* **2016-07-18** I [did an entire run](http://www.youtube.com/watch?v=7c5wCzQO5po) using ChoregraphAI strats, improving on my previous World Record by more than a second.

* **2016-07-10** ChoregraphAI found [a surreal route](https://www.youtube.com/watch?v=XbH2m-7lfSk). It moves back and forth for a while, perfectly manipulating several unseen enemies in position so that there’s an easy way to the exit. This is two beats faster than any human-found route for this level.

* **2016-06-20** ChoregraphAI found [its first route](https://www.youtube.com/watch?v=dNYfj2hQ3kI&feature=youtu.be).

* **2016-06-16** ChoregraphAI went open source, with the permission of NecroDancer’s creators. At this point it was only able to solve short levels, and didn’t know how to use any items.

* **2016-05-09** Initial (unusable) version of ChoregraphAI.

## License

This work is licensed under the MIT License.
