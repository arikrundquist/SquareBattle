
#ifndef GAME_API_H
#define GAME_API_H

#include <unistd.h>

enum class action_t : uint8_t {
    NONE = 0,
    HIDE,
    MOVE,
    ATTACK,
    SPAWN,
    REPLICATE,
    WATCH,
    UPGRADE,
};

enum class direction_t : uint8_t {
    NONE = 0b0000,
    NORTH = 0b1000,
    SOUTH = 0b0100,
    WEST = 0b0010,
    EAST = 0b0001,
    NORTHEAST = NORTH|EAST,
    NORTHWEST = NORTH|WEST,
    SOUTHEAST = SOUTH|EAST,
    SOUTHWEST = SOUTH|WEST,
};

enum class upgrade_t : uint8_t {
    BLITZ = 1,
    LIGHT = 2,
    HEAVY = 3,
};

class Action {
    inline Action(action_t action, uint8_t params) : action(_action_(action, params)) { };
    inline Action(action_t action, direction_t direction) : Action(action, (uint8_t) direction) { };
    inline Action(action_t action, upgrade_t upgrade) : Action(action, (uint8_t) upgrade) { };
    static inline uint8_t _action_(action_t act, uint8_t params) { return ((uint8_t) act << 4) | params; }
public:
    const uint8_t action;
    static inline Action MOVE(direction_t direction) { return Action(action_t::MOVE, direction); }
    static inline Action ATTACK(direction_t direction) { return Action(action_t::ATTACK, direction); }
    static inline Action SPAWN(direction_t direction) { return Action(action_t::SPAWN, direction); }
    static inline Action REPLICATE(direction_t direction) { return Action(action_t::REPLICATE, direction); }
    static inline Action WATCH(direction_t direction) { return Action(action_t::WATCH, direction); }
    static inline Action UPGRADE(upgrade_t upgrade) { return Action(action_t::UPGRADE, upgrade); }
    static const Action NONE, HIDE;
};

struct Square {
    virtual Action act() { return Action::NONE; };
    virtual ~Square() = default;
};

#endif
