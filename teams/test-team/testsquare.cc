
#include "../../api/square.h"

class TestSquare : public Square {

    virtual Actions act(params) override {
        return Action::MOVE(direction_t::NORTH);
    }
};

const char *display_name() { return "test square"; }
const char *victory() { return "yay i won"; }
const char *defeat() { return "oh sad i lost"; }

Square *start(int team, size_t board_size) { return new TestSquare; }
