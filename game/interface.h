
#ifndef INTERFACE_H
#define INTERFACE_H

#include "../api/square.h"

struct SerializedSquare {
    Square *square;
    size_t x, y;
    SquareData data;
};

struct Message {
    enum {
        SQUARE,
        DESTROY,
    } type;
    union u {
        void *_;
        SerializedSquare square;

        u() : _(0) { }
    } data;

    Message() : type(SQUARE) { }
};

struct SerializedAction {
    Square *square;
    size_t x, y;
    Action action;
};

struct Response {
    enum {
        ACTION,
        INVALID,
    } type;
    union u {
        void *_;
        SerializedAction action;

        u() : _(0) { }
    } data;

    Response() : type(ACTION) { }
};

#endif
