
#ifndef SQUARE_H
#define SQUARE_H

#ifdef IMPORT
#define API(type, name) type (*name)
#else
#define API(type, name) extern "C" __attribute__((visibility("default"))) type name
#endif

#include <vector>

#include "api.h"

// for convenience
class Actions {
    std::vector<Action> actions;
public:
    template <typename... T>
    Actions(T... actions) : actions(std::vector<Action>{actions...}) { }
    auto begin() { return this->actions.begin(); }
    auto end() { return this->actions.end(); }
};

// also for convenience
#define params const int view[3][3], const size_t pos[2], const SquareData &self, Square *&spawn

// basic interface, inherit from this
struct Square {
    virtual Actions act(params) {
        return Action::NONE();
    }
    virtual void destroyed(const size_t pos[2]) { }
    virtual ~Square() { }
};

// required
API(Square *, start)(int team, size_t board_size);

// optional
API(const char *, display_name)();
API(const char *, victory)();
API(const char *, defeat)();

#endif
