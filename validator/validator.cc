
#include "../game/board_size.h"

#include "validator.h"

#include "../utils/Logger.h"

#define LOG(...) //Logger::log(__VA_ARGS__)
#define DEBUG LOG("%s %d\n", __FUNCTION__, __LINE__);

struct Square {
    
    // double linked list
    Square *prev = nullptr;
    Square *next = nullptr;

    // square state (matches SquareData)
    uint8_t cooldown = 0;
    uint8_t health = 1;
    uint8_t max_actions = 1;
    uint8_t current_actions = 0;
    uint8_t action_interval = 0;
    uint8_t stealth = 0;
    uint8_t can_attack = 1;
};

#define mod(x) ((x + BOARD_SIZE) % BOARD_SIZE)

class RuleValidator : public Validator {
    Square *first = nullptr;
    Square *state[BOARD_SIZE][BOARD_SIZE] = {0};
    Square *next_state[BOARD_SIZE][BOARD_SIZE] = {0};
    uint8_t attack_amount[BOARD_SIZE][BOARD_SIZE] = {0};
    uint8_t collision_health[BOARD_SIZE][BOARD_SIZE] = {0};
    uint8_t num_to_destroy[BOARD_SIZE][BOARD_SIZE] = {0};

    Square *active = nullptr;
    size_t sx, sy, cx, cy;

    void remove(Square *s) {
        if(s->prev) s->prev->next = s->next;
        else {
            if(s != first) bad("invalid list");
            first = s->next;
        }
        if(s->next) s->next->prev = s->prev;
        delete s;
    }

    void place(Square *s, size_t x, size_t y) {
        auto other = next_state[x][y];
        if(!other) {
            uint16_t damage = 0;
            damage += attack_amount[x][y];
            damage += collision_health[x][y];
            uint16_t health = s->health;
            LOG("placing %ld %ld (hp=%d, damage=%d)\n", x, y, health, damage);
            if(health > damage) next_state[x][y] = s;
            else if(damage == 0 && num_to_destroy[x][y] == 0) next_state[x][y] = s;
            else {
                collision_health[x][y] = std::max(collision_health[x][y], s->health);
                num_to_destroy[x][y]++;
                remove(s);
            }
            return;
        }
        num_to_destroy[x][y]++;
        LOG("placing %ld %ld (hp=%d, other=%d)\n", x, y, s->health, other->health);
        if(s->health == other->health) {
            num_to_destroy[x][y]++;
            collision_health[x][y] = std::max(collision_health[x][y], s->health);
            remove(s);
            remove(other);
            next_state[x][y] = nullptr;
            return;
        }
        if(s->health < other->health) {
            collision_health[x][y] = std::max(collision_health[x][y], s->health);
            remove(s);
            return;
        }
        if(other->health < s->health) {
            collision_health[x][y] = std::max(collision_health[x][y], other->health);
            remove(other);
            next_state[x][y] = s;
            return;
        }
    }

    void bad(const char *message) {
        printf("frame %d: %s\n", this->frame(), message);
        exit(-1);
    }
    inline void check_actions() {
        if(active->cooldown || !active->current_actions--) bad("square should have no actions");
    }
#define check_adjacency(x, y, action) if(x % BOARD_SIZE != x || y % BOARD_SIZE != y) bad(action " outside of board"); if((mod(x - cx) + 1) % BOARD_SIZE > 2 || (mod(y - cy) + 1) % BOARD_SIZE > 2) bad(action " non-adjacent square")
public:
    RuleValidator() = default;
    virtual ~RuleValidator() = default;

    virtual void start(size_t x, size_t y) override {
        LOG("%s %ld %ld\n", __func__, x, y);

        auto s = new Square;
        s->next = first;
        if(first) first->prev = s;
        first = s;
        if(state[x][y]) bad("two teams started at the same location");
        state[x][y] = s;
    }
    virtual void activate_square(size_t x, size_t y, void *squaredata) override {
        
        LOG("%s %ld %ld\n", __func__, x, y);

        if(!(active = state[x][y])) bad("non-existant square");
        
        uint8_t *data = (uint8_t *) squaredata;
#define check(element) if(active->element != *(data++)) {printf("found %d but expected %d\n", *(data-1), active->element); bad("square " #element " incorrect");}
        check(cooldown);
        check(health);
        check(max_actions);
        check(current_actions);
        check(action_interval);
        check(stealth);
        check(can_attack);
#undef  check
                
        state[x][y] = nullptr;
        sx = x; sy = y;
        cx = sx; cy = sy;
    }
    virtual void attack(size_t x, size_t y, bool try_attack) override {
        LOG("%s %ld %ld\n", __func__, x, y);

        check_actions();
        check_adjacency(x, y, "attacking");
        if(!try_attack) return;
        if(try_attack && !active->can_attack) bad("square should not be able to attack");
        if(!++attack_amount[x][y]) attack_amount[x][y]--;
    }
    virtual void destroyAt(size_t x, size_t y) override {
        LOG("%s %ld %ld\n", __func__, x, y);

        if(!num_to_destroy[x][y]--) bad("should not have destroyed square");
    }
    virtual void hide() override {
        LOG("%s\n", __func__);

        check_actions();
        if(!++(active->stealth)) active->stealth--;
    }
    virtual void move(size_t x, size_t y) override {
        LOG("%s %ld %ld\n", __func__, x, y);

        check_actions();
        check_adjacency(x, y, "moving to");
        cx = x; cy = y;
        active->stealth /= 2;
    }
    virtual void spawn(size_t x, size_t y, bool valid) override {
        LOG("%s %ld %ld\n", __func__, x, y);

        check_actions();
        check_adjacency(x, y, "spawning");
        active->cooldown = 99;
        active->current_actions = 0;
        if(!valid) return;
        auto s = new Square;
        s->cooldown = 99;
        s->next = first;
        first->prev = s;
        first = s;
        place(s, x, y);
    }
    virtual void replicate(size_t x, size_t y, bool valid) override {
        LOG("%s %ld %ld\n", __func__, x, y);

        check_actions();
        check_adjacency(x, y, "replicating");
        active->cooldown = 99;
        active->current_actions = 0;
        if(!valid) return;
        auto s = new Square;
        *s = *active;
        s->prev = nullptr;
        s->next = first;
        first->prev = s;
        first = s;
        place(s, x, y);
    }
#define inc(x) if(!++(x)) x--
    virtual void blitz() override {
        LOG("%s\n", __func__);

        check_actions();
        active->cooldown = 99;
        active->current_actions = 0;
        inc(active->max_actions);
        inc(active->action_interval);
    }
    virtual void heavy() override {
        LOG("%s\n", __func__);

        check_actions();
        active->cooldown = 99;
        active->current_actions = 0;
        inc(active->health);
        inc(active->health);
        inc(active->action_interval);
    }
    virtual void light() override {
        LOG("%s\n", __func__);

        check_actions();
        active->cooldown = 99;
        active->current_actions = 0;
        if(!active->health) return;
        active->can_attack = 0;
        inc(active->max_actions);
        active->health--;
    }
#undef inc
    virtual void finalize_square() {
        LOG("%s\n", __func__);

        place(active, cx, cy);
        active = nullptr;
    }
    virtual void finish_frame() override {
        LOG("%s\n", __func__);

        for(size_t i = 0; i < BOARD_SIZE; i++) {
            for(size_t j = 0; j < BOARD_SIZE; j++) {
                if(next_state[i][j]) {
                    auto s = next_state[i][j];
                    next_state[i][j] = nullptr;
                    place(s, i, j);
                    if(next_state[i][j]) next_state[i][j]->health -= attack_amount[i][j] + collision_health[i][j];
                }
                state[i][j] = next_state[i][j];
                next_state[i][j] = nullptr;
                attack_amount[i][j] = 0;
                collision_health[i][j] = 0;
            }
        }
    }
protected:
    virtual void start_frame() override {
        LOG("%s %ld\n", __func__, this->frame());

        for(auto s = first; s; s = s->next) {
            if(!s->action_interval || frame() % s->action_interval == 0) s->current_actions = s->max_actions;
            if(s->cooldown) s->cooldown--;
        }
        for(size_t i = 0; i < BOARD_SIZE; i++) {
            for(size_t j = 0; j < BOARD_SIZE; j++) {
                if(num_to_destroy[i][j]) {
                    printf("need to destroy %d at %ld %ld\n", num_to_destroy[i][j], i, j);
                    bad("failed to destroy a square");
                }
            }
        }
    }
};

template <>
Validator *Validator::make<RuleValidator>() {
    return new RuleValidator();
}
