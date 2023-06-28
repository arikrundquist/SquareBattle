
[Spec, tentative]

Overview:
  1. players get a single object at the start of the game
  2. each frame, players receive information about all of their objects
  3. each object must provide an action that it takes

Timing:
  0. players get up to 1s for initialization
  1. players get up to 10ms + 2ms per object (eg. 1000 objects gets 2010ms)
     - may be adjusted later
  2. if a player times out, they take no further actions that iteration
     - player receives a notification that they timed out

Map:
  1. map is a square grid, where each side connects to the opposite side
  2. each cell has several properties:
     - occupant (a team id, or 0 if unoccupied)

Objects:
  1. {
       max_actions = 1,
       current_actions = 1,
       action_interval = 1,
       health = 1,
       can_attack = true,
       can_watch = 0,
       watching = NORTH|SOUTH,
       stealth = 0,
     }
  2. may upgrade objects to produce multiple types
     - blitz
       - able to surge forward at the cost of a cooldown
       - max_actions++
       - action_interval++
     - light
       - quick, but defenseless
       - max_actions++
       - health--
       - can_attack = false
     - heavy
       - increased health, but slower
       - action_interval++
       - health++
     - watcher
       - able to watch adjacent tiles for enemies
       - on enter, uses all remaining actions to attack
       - can_watch++
  3. can combine upgrade types
     - eg. a heavy blitz would have two health and get a max of two actions every three turns
     - note the effective limit of eight watcher upgrades, since a square can only watch up to eight adjacent tiles

Actions:
  0. none (cost 0)
  1. move in any direction (including diagonals, cost 1)
  2. attack in any direction (cost 1)
  3. hide (while not taking other actions, increase chance of not being seen, cost 1)
  4. replicate in any direction (cost 100, vulnerable until fully paid)
  5. upgrade to another type (cost 100, vulnerable until fully paid)
  6. watch an adjacent tile for enemies (reset on move, attack, replicate, upgrade, cost 1)

Resolution:
  1. actions occur in stages
     - all internal state is updated (eg. receiving action points, finishing an upgrade, hiding, watching)
     - movement occurs simultaneously and collisions are resolved
     - attacking and replicating occurs simultaneously
  2. only end positions are checked for collisions
     - this allows objects to swap positions
     - also allows objects that move more than one tile to jump over other objects
     - on collision, only the object with the most health remains, losing health equal to the second healthiest object (both destroyed if tie)
  3. objects with multiple actions only receive info for their starting location
  4. objects may move through spaces that are being attacked since movement is resolved first

Communication:
  1. queue of messages (object data) and responses (action)
