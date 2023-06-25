
#ifndef SQUARE_H
#define SQUARE_H

#ifdef IMPORT
#define API(type, name) type (*name)
#else
#define API(type, name) extern "C" __attribute__((visibility("default"))) type name
#endif

#include <vector>

#include "api.h"

struct Square {
    virtual std::vector<Action> act(const int view[3][3], const int pos[2], const SquareData &self, Square *&spawn) {
        return std::vector<Action>{Action::NONE};
    }
    virtual ~Square() { }
};

API(const char *, victory)();
API(const char *, defeat)();

API(Square *, start)(int team);

#endif
