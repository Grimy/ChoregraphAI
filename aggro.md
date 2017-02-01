Aggro and activation
====================

There are two reasons why an enemy might move: **aggro** and **activation**.
Enemies that are aggroed, activated or both will try to move. Enemies that are
neither aggroed nor activated will stand still.

Aggro happens when you can *see* the enemy. Activation happens when you are
*close* to the enemy. See later sections for the technical details.

Aggro is permanent. Once an enemy is aggroed, the only way to de-aggro it to
confuse it, and be out of sight when the confusion ends.

Activation is transient. If you move far enough from an enemy, it will
immediately de-activate. It may of course keep moving if it was also aggroed.

Enemies will often play a sound when they become aggroed (banshee wails, dragon
roars…), but not when they become activated.

Nightmares and dragons that are activated but not aggroed will **trample** if
they are blocked from moving toward you. Trampling affects the 4 tiles adjacent
to the dragon/nightmare, destroying walls and dealing 4 normal damage to anything
that was standing there.

Non-player bombs and exploding gargoyles do not cause any damage to non-aggroed enemies.

More about activation
---------------------

Each enemy has an **activation radius**, which is a non-negative integer. The
enemy is activated only when you are within a *circle* of that radius, centered on the
enemy (formally: iff L² distance ≤ radius).

Example: activation radii 7 and 3, visualized using ice tiles.

![Activation radii](http://i.imgur.com/C5wn58a.png)

Slimes and zombies have an “infinite” radius: they are always active.

Harpies, all mages, goblin bombers, evil eyes, blue dragons, earth dragons and
headless skeletons have a radius of 0: they will only move when aggroed.

Green dragons and minotaurs have a radius of 7. Nightmares have a radius of 9.
Red dragons have a radius of 10.

For all other enemies, the activation radius is zone-dependant. It’s 5 in Z2, 7
in Z3, and 3 in the other zones.

More about aggro
----------------

An enemy becomes aggroed if it stands on (or moves to) a **revealed** tile.

Revealed tiles are those tiles shown on your minimap (without mapping). A tile
becomes revealed if it’s within your **line-of-sight** and receives enough
**lighting**. Additionally, torches, miner’s cap and ring of darkness let you
“see through walls”: all tiles in a circle around you are revealed, even if you
don’t have line-of-sight to those tiles.

Line-of-sight: draw straight lines from the center of your current tile to the
four corners of another tile. You have line-of-sight to that tile iff at least
one of those lines doesn’t intersect any walls. (I’m skipping over many details
here, but that’s the heart of the algorithm).

Lighting: the total lighting of a tile is the *sum* of the lighting it
receives from nearby light sources (it’s not obvious that it should be a sum:
Minecraft, for example, uses a max). The light provided by an individual source
is equal to the brightness of the source minus the square of the distance. When
the total lighting exceeds some fixed threshold, the tile is considered “lit”.

Take the example of a wall torch:

* At distance 2*sqrt(2) (two tiles away diagonally), it single-handedly provides enough light to reveal a tile.
* At distance 3, it’s just barely under the threshold.
* At distance 4, it provides very little light; however, combined with another wall-torch at distance 3, it’s enough to reveal a tile.
* At distance 5, it doesn’t provide any light at all.

Torch items still provide light while lying on the ground, but less than while
you are holding them. Light mushrooms are brighter than wall torches.

Tiles inside a nightmare’s shadow are an exception. They’re considered fully
lit for aggro purposes, but not for mapping purposes.

Note that freeze is completely independent from aggro. Frozen enemies can still
aggro, and the freeze comes off at the same pace even for unaggroed,
unactivated enemies.

Enemies spawned by mommies, sarcophagi, electro-liches and skulls start out
already aggroed, even if the enemy that spawned them wasn’t.

Weird interactions / bugs
-------------------------

If a sarcophagus is aggroed and activated on the same beat, it “misses its
turn”: it doesn’t spawn anything, but still resets to the first beat of its
cycle.

If an enemy is aggroed without being activated, it will usually wait a beat
before doing anything. There are, of course, exceptions:

* Dragons will move immediately (easy to test with blue dragons)
* If the enemy is inside a nightmare’s shadow, it will act immediately
* If a player bomb or gargoyle exploded during the beat, the enemy will act immediately (this happens no matter where the explosion occured)

There are, of course, exceptions to the exceptions. Enemies with activation
radius 0 are immune to the latter two bugs.

[Video demonstrating the bomb-aggro bug](https://www.youtube.com/watch?v=0yCduza7eDQ).
