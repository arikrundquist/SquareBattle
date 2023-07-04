
#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <unistd.h>

class Validator {
    size_t framenum = 0;
protected:
    inline size_t frame() { return framenum; }
    virtual void start_frame() { }
public:
    template <typename T>
    static Validator *make();

    void frame(size_t framenumber) {
        framenum = framenumber;
        start_frame();
    }
    virtual void start(size_t x, size_t y) { }
    virtual void activate_square(size_t x, size_t y, uint8_t health) { }
    virtual void attack(size_t x, size_t y, bool can_attack=true) { }
    virtual void destroyAt(size_t x, size_t y) { }
    virtual void hide() { }
    virtual void move(size_t x, size_t y) { }
    virtual void spawn(size_t x, size_t y, bool valid=true) { }
    virtual void replicate(size_t x, size_t y, bool valid=true) { }
    virtual void blitz() { }
    virtual void heavy() { }
    virtual void light() { }
    virtual void finalize_square() { }
    virtual void finish_frame() { }

    virtual ~Validator() = default;

};

class RuleValidator;

#endif
