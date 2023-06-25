
#include "../../api/square.h"
#include "../../api/api.cc"

#include <iostream>

class TestSquare : public Square {

    virtual std::vector<Action> act(const int view[3][3], const int pos[2], const SquareData &self, Square *&spawn) override {
        return std::vector<Action>{Action::MOVE(direction_t::NORTH)};
    }
};

const char *victory() { return "yay i won"; }
const char *defeat() { return "oh sad i lost"; }

Square *start(int team) { return new TestSquare; }
