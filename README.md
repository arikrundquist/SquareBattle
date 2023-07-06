
# Square Battle

### What is it?

A game and coding challenge inspired by [Endigit's Square Battle](https://endigit.com/squarebattle) (note: the site's certificate appears to have expired).

### Purpose

To be a fun way to practice C/C++ in a sandboxed environment. Square Battle is unique in that it appeals to individuals across a broad spectrum of expertise in these languages. Even simple strategies may perform well, but players may also elect to spend countless hours tuning and perfecting their team via complex data structures and algorithms.

### Project structure

Square Battle is intended to be run inside Docker containers. Why? Because players may submit arbitrary code to power their teams. While the original Square Battle for LabVIEW assumed players were well-intentioned and made no attempt to sandbox them, with C/C++ being the low level languages they are, players have immense flexibility but also the potential to do real, lasting damage to the system. Since I have elected to allow players to run arbitrary build steps, I must also sandbox the build processes of each team.

The code for the game engine is contained in the `game` directory. `gui.py` provides a graphical wrapper to the backend, allowing users to choose which teams to run. It also allows users to set the seed in order to ensure deterministic results (this is useful for players to debug their team, as well as for contributers to reproduce bugs consistently). `backend.cc` takes its arguments from the gui wrapper and spawns separate processes for each team, placing them inside their own [chroot jail](https://en.wikipedia.org/wiki/Chroot) so that they cannot cheat by accessing game state or by attempting to inspect the other teams.

The backend then passes control to `server`, which holds all of the game logic. `server` is, unfortunately, very messy and is in desparate need of cleaning up (I mean, it's not *awful*, but it's certainly not great). Eventually, I would like to refactor `server` to make it unit testable, but for now, I rely on a separate implementation of the game -- `validator/validator.cc` -- to double check what `server` is doing. Together, these handle receiving actions for each teams' squares and updating game state, populating a pixel buffer to be presented by `graphics.py`.

### Getting started

To run Square Battle, you must first install both [docker](https://www.docker.com/) and an X11 server (I use [Xming](https://sourceforge.net/projects/xming/); its defaults should be fine, except you will need to disable access control unless you know how to authenticate X11... I don't!).

Docker will also install [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/install), although it will set the default linux installation to something Docker related, causing the `wsl` command to fail when you run it manually. To build the project, you need `make`, so you will either need a windows port of `make` (from [Cygwin](https://www.cygwin.com/), for example), or another linux installation for `wsl`. `wsl` is probably the simplest option: running `wsl install ubuntu` followed by `wsl -s Ubuntu` should be sufficient. After running these commands, run `wsl` in a terminal to access the Ubuntu environment. Now you can run `make`; it will fail, but it will helpfully prompt you to run `apt install make` in order to install the utility.

Now, at last, everything should work. Running `make` will now prompt you to populate `ip.txt` with your ip address so that the X11 graphics client inside the Docker container knows where to foward its packets.

Run the game with `make run`. If it complains about not finding X11 but shows no other errors, you may need to try a different ip if your machine is configured with multiple. Otherwise, you should see a new X11 window open and you're all set!

### Making a team

##### Bare minimum

Now that you are able to run the game, try your hand at making a team. Create a new folder inside the `teams` directory. The name of the folder is how your team will appear in the gui launcher, and cannot start with a `.` (this is intended to eventually allow "hidden" teams to use for integration tests without cluttering the gui dropdowns).

Everything in your team's folder is yours to do with as you please. There are only two requirements:

1. you must implment the `start` function defined in `api/square.h`, returning an instance of your starting square, and
2. your team should build with `make team.so` at the top of your team's folder.

A good example of a bare-bones team is found in `teams/test-team`. This team only moves north. A more interesting, but still trivial, team is `teams/random-team`, which showcases the variety of actions your squares can take.

##### Improving your team

This is the fun part! Override the `act` and `destroyed` methods of your square to look at the game state provided to you and react accordingly. Also, don't feel limited to only implementing a single type of square -- you may find your team benefits from having a variety of specialized squares each acting in distinct ways.

###### Parameters
1. `act` (note: the parameters for this function are kind of unwieldy, so the macro `params` is used instead... feel free to use this macro in your own square subclasses while overriding this method)
  - `const int view[3][3]`: the 3x3 grid around the active square at the start of the round (row-major order where `view[0][0]` is north-west). Each cell contains the team id of the square inside it, or 0 if the cell is empty.
  - `const size_t pos[2]`: the `xy` position of the square (note: the top of the grid is `y=0`; also note that the `xy` order is not the same as the row-major order of `view`, where `y` comes first).
  - `const SquareData &self`: information about the current square:
    - `cooldown`: always 0 (since `act` is not called on squares that still have a cooldown).
    - `health`: the current health of the square.
    - `max_actions`: the maximum number of actions this square can ever take.
    - `current_actions`: the number of actions this square has remaining.
    - `action_interval`: the number of frames between resetting `current_actions` to `max_actions`.
    - `stealth`: the odds (0-255) that this square does not appear in the `view` of adjacent squares.
    - `can_attack`: boolean 0 or 1 specifying whether the current square may attack.
  - `spawn`: set this to a new square instance if you take the `spawn` or `replicate` action.
2. `destroyed`
  - `const size_t pos[2]`: same as above.

###### Actions

Unlike the original Square Battle, squares in this version may, through upgrades, take multiple actions in a single turn. However, since most squares, at least in the early game, may only take a single action, the wrapper class `Actions` was created. This class handles some basic conversions, allowing players to return either a single `Action` or an `std::vector<Action>`. `Actions` are constructed with a variable number of arguments, so `return Actions(action1, action2, ..., actionN);` will also work.

The LabVIEW Square Battle allowed only four actions: `NONE`, `ATTACK`, `MOVE`, and `REPLICATE`. I have expanded this list to provide for a greater diversity of strategies. Unless otherwise specified, each option counts as a single action to be subtracted from the square's `current_actions`.

1. `Action::NONE()`: noop, does not expend an action.
2. `Action::HIDE()`: increment the square's stealth.
3. `Action::MOVE(direction)`: move a single cell in the specified direction.
3. `Action::ATTACK(direction)`: deal one damage in the specified direction.
3. `Action::SPAWN(direction)`: create a new, blank square (ie. without upgrades) in the specified direction. The `spawn` parameter must also be set to a square instance.
3. `Action::REPLICATE(direction)`: duplicate this square (including upgrades) in the specified direction. The `spawn` parameter must also be set to a square instance.
3. `Action::UPGRADE(upgrade)`: gain one of the following upgrades (note: upgrades stack):
  - `BLITZ`: the most versitile upgrade, this increments both the `max_actions` and `action_interval` of the square. A default square taking this upgrade, for example, receives two actions every other turn. It may take a single action each turn, or it may use both actions on one of the turns.
  - `LIGHT`: a fairly niche upgrade, this decrements the square's `health` but increments its `max_actions`. This may drop the square's health to 0 but does not kill the square. If the square already has 0 health, this upgrade is wasted. Squares that have taken the `LIGHT` upgrade may not attack, nor may squares that have been replicated from a `LIGHT` square.
  - `HEAVY`: increases the square's health by two but also increments `action_interval` by one.

Taking the `SPAWN`, `REPLICATE`, or `UPGRADE` actions immediately ends the square's turn; both the square and any newly created square must cooldown for 100 frames, during which time `act` is not called.

Actions do not accumulate. That is, when a square receives actions, it's `current_actions` are reset to `max_actions` -- `current_actions` is *not* incremented by that amount.

### Notes on action resolution

The original Square Battle resolved each square sequentially in a randomized order. This meant that if two adjacent squares attacked each other, whichever acted first would destroy the other without being harmed.

In this implementation, all actions are resolved simultaneously. This means that for the previous example, both squares would be destroyed. It also means that squares may swap places; collisions only occur if two squares end their turn in the same place. Each frame, squares take damage equal to the total number of attacks that targeted its final position plus the health of the second strongest square in the cell. For example, if a square with health=5 and a square with health=2 moved into a square that had been attacked once, the weaker square would be destroyed and the stronger square would take three total damage. (Note: there is currently an unhandled edge case where if several squares with health=0 collide, the last square may not be destroyed since it takes no damage from the earlier collisions; this is low priority, but will probably be resolved eventually).

Although a square may take multiple actions in a turn (assuming it has the upgrades to do so), the square does not gain any new information about the state of the board between actions. A square that moves twice, for example, may move right past an enemy square with neither the wiser.

###### I found a bug, now what?

Open an issue and I'll find time to look into it. Or, if you're feeling adventurous, poke around the code and open a pull request with the fix.

### But above all else

Just have fun! This should be an exciting competition, and I'm excited to see what you all come up with!
