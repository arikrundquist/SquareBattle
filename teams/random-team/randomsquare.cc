
#include "../../api/square.h"

#include <stdlib.h>
#include <stdio.h>

direction_t rd() {
    switch(rand() % 9) {
    default:
    case 0: return direction_t::NONE;
    case 1: return direction_t::EAST;
    case 2: return direction_t::WEST;
    case 3: return direction_t::SOUTH;
    case 4: return direction_t::NORTH;
    case 5: return direction_t::SOUTHEAST;
    case 6: return direction_t::SOUTHWEST;
    case 7: return direction_t::NORTHEAST;
    case 8: return direction_t::NORTHWEST;
    };
}
upgrade_t ru() {
    switch(rand() % 3) {
    default:
    case 0: return upgrade_t::BLITZ;
    case 1: return upgrade_t::HEAVY;
    case 2: return upgrade_t::LIGHT;
    };
}

class RandomSquare : public Square {

    static Action randomAction() {
        auto d = rd();
        auto u = ru();

        switch(rand() % 7) {
        default:
        case 0: return Action::NONE();
        case 1: return Action::HIDE();
        case 2: return Action::MOVE(d);
        case 3: return Action::ATTACK(d);
        case 4: return Action::SPAWN(d);
        case 5: return Action::REPLICATE(d);
        case 6: return Action::UPGRADE(u);
        }
    }

    virtual Actions act(params) override {
        spawn = new RandomSquare;

        size_t r = rand();
        r = r % (self.current_actions << 1 + 1);
        std::vector<Action> actions{};
        while(r--) {
            actions.push_back(RandomSquare::randomAction());
        }
        return actions;
    }
};

Square *start(int team, size_t board_size) {
    srand(rand() + team);
    return new RandomSquare;
}

const char *victory() { return "random for the win"; }
