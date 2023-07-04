
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
        DELETE,
        FRAME_START,
        FRAME_END,
        CLOSE,
    } type;
    union u {
        void *_;
        struct {
            Square *square;
            size_t x, y;
        } destroyed;
        struct {
            SerializedSquare data;
            int view[3][3];
        } square;
        size_t num;

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
        INITIAL,
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
