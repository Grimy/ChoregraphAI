## The Order of Events

It’s easy to think certain things “happen at the same time”. But there’s an order to everything.

A run is divided in **beats**. Beats are tied to real time, making their order straightforward: there’s never a risk
of thinking two distinct beats happen at the same time.

Each beat is divided in **turns**, where individual entities get to act: the player’s turn, enemy turns, trap turns…
The order of turns within a beat is discussed in section 1.

Some turns involving complex actions are further divided. For example, bombs and other multi-tile effects
affect tiles one by one in a given order. This is discussed in section 2.

### 1. Turn Order

The general structure of a beat is as follow:

* Player’s turn
* Every-frame updates
* Bombs’ turns
* Enemies’ turns
* Enemy digging
* Coals/ice processing
* Traps’ turns 
* Every-frame updates

#### 1.1 Player’s turn

Quite a lot of things happen during the player’s turn:

* Attack
* On-hit effects
* Movemevent (including axe/cat/rapier and gun recoil)
* On-move effects

#### 1.2 Every-frame updates

Quite a lot of things happen every frame. Since all the enemy/traps/… are
processed in a single frame, this means that for all intents and purposes, they
happen twice a beat: immediately after the player’s move, and at the very

* Updating the field of view (matters for aggro)
* Closing metal doors
* Re-growing Z5 walls
* Unlocking stairs
* Beetles shedding
* Mimics and gargoyles waking up
* Super-secret-shopkeeper

This has a number of interesting consequences. For example, if the miniboss is
killed by a bomb or trap while the player is standing on the locked stairs, the player
won’t gain end-of-level invincibility until the start of the next beat.

Metal door timing has some funny interactions with blademasters:

Those every-frame 
TODO: couraging miniboss while on the stairs
TODO: armadillo, blademaster, w/ and w/o ring of war.

#### 1.2 Player’s turn

