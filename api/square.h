
#ifndef SQUARE_H
#define SQUARE_H

#ifdef IMPORT
#define API(type, name) type (*name)
#else
#define API(type, name) extern "C" __attribute__((visibility("default"))) type name
#endif

#include <vector>

#include "api.h"

#define params const int view[3][3], const size_t pos[2], const SquareData &self, Square *&spawn

struct Square {
    virtual std::vector<Action> act(params) {
        return std::vector<Action>{Action::NONE()};
    }
    virtual void destroyed(const size_t pos[2], const int other_team) { }
    virtual ~Square() { }
};

API(const char *, display_name)();
API(const char *, victory)();
API(const char *, defeat)();

API(Square *, start)(int team);

#endif
