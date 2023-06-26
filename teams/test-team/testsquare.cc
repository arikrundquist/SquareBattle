
#include "../../api/square.h"

class TestSquare : public Square {

    virtual std::vector<Action> act(params) override {
        return std::vector<Action>{Action::MOVE(direction_t::NORTH)};
    }
};

const char *display_name() { return "name"; }
const char *victory() { return "yay i won"; }
const char *defeat() { return "oh sad i lost"; }

Square *start(int team) { return new TestSquare; }
