
#include <thread>
#include <list>
#include <deque>
#include <queue>
#include <tuple>

#include "server.h"
#include "../utils/Timer.h"
#include "../utils/Logger.h"

#define LOG(...) //Logger::log(__VA_ARGS__)
#define DEBUG LOG("%s %d\n", __FUNCTION__, __LINE__);

Validator *validator = nullptr;

class Team;
struct TeamSquare;
struct {
    Team *team;
    TeamSquare *square;
} team_positions[BOARD_SIZE][BOARD_SIZE] = {0};
uint16_t attacked[BOARD_SIZE][BOARD_SIZE] = {0};
uint8_t second_most_health[BOARD_SIZE][BOARD_SIZE] = {0};
bool collisions[BOARD_SIZE][BOARD_SIZE] = {0};

// returns two integers, f1 and f2 such that the product
// of the two integers is the given amount, f1 > f2,
// and f1 and f2 are as close as possible
std::pair<size_t, size_t> factorize(size_t amount) {
    size_t f1 = 1, f2 = amount;

    for(size_t i = 1; f1 < f2; i++) {
        if(f2 % i) continue;
        if(i == f2) {
            f2 = f1;
            f1 = i;
            continue;
        }
        f1 *= i;
        f2 /= i;
    }

    return std::pair<size_t, size_t>(f1, f2);
}

// apply a direction offset to an x, y position
// this handles wrapping around the board
void offset(size_t &x, size_t &y, direction_t dir) {
    if((uint8_t) dir & (uint8_t) direction_t::NORTH) {
        y = (y + BOARD_SIZE - 1) % BOARD_SIZE;
    }
    if((uint8_t) dir & (uint8_t) direction_t::SOUTH) {
        y = (y + 1) % BOARD_SIZE;
    }
    if((uint8_t) dir & (uint8_t) direction_t::EAST) {
        x = (x + 1) % BOARD_SIZE;
    }
    if((uint8_t) dir & (uint8_t) direction_t::WEST) {
        x = (x + BOARD_SIZE - 1) % BOARD_SIZE;
    }
}

struct TeamSquare {
    Square *square;
    SquareData data;
    size_t x, y;
    std::deque<std::pair<Action, Square *>> actions;
};

void print(const TeamSquare &square) {
    printf(
        "%ld %ld @ %p\n"
        "  cooldown %d\n"
        "  health %d\n"
        "  max_actions %d\n"
        "  current_actions %d\n"
        "  action_interval %d\n"
        "  stealth %d\n"
        "  can_attack %d\n"
    , square.x, square.y, square.square,
    square.data.cooldown, square.data.health,
    square.data.max_actions, square.data.current_actions,
    square.data.action_interval, square.data.stealth, square.data.can_attack);
}

class Team {
    const int teamid;
    ProcessQueue *queue;
    std::deque<TeamSquare> squares;
    static inline TeamSquare spawn(Square *square, size_t x, size_t y) {
        return {
            .square = square,
            .data = {
                .cooldown = 0,
                .health = 1,
                .max_actions = 1,
                .current_actions = 1,
                .action_interval = 0,
                .stealth = 0,
                .can_attack = 1
            },
            .x = x, .y = y,
            .actions = std::deque<std::pair<Action, Square *>>{},
        };
    }
public:
    inline int id() const { return teamid; }
    Team(int teamid, ProcessQueue *queue, Square *square, size_t x, size_t y) : teamid(teamid), queue(queue) {
        TeamSquare first = spawn(square, x, y);
        this->squares.push_back(first);
        team_positions[y][x] = {
            .team = this,
            .square = &this->squares.front(),
        };
    }

    // send a message to the team process
    inline void send(const Message &message) {
        this->queue->send(message);
    }

    // receive a response from the team process
    inline Response receive() {
        Response response;
        this->queue->receive(response);
        return response;
    }

    // populate all squares with their actions
    void get_actions() {

        // enqueue requests in separate thread
        auto message = std::thread([this] {

            // i want to time how long this takes
            Timer frame;
            Message m;

            // notify frame start
            m.type = Message::FRAME_START;
            this->send(m);

            // send data for each of the squares
            m.type = Message::SQUARE;
            for(const auto & square : this->squares) {
                
                // if the square can't do anything, just skip it
                if(square.data.cooldown || !square.data.current_actions) continue;

                // send the data
                m.data.square.data = {
                    square.square,
                    square.x,
                    square.y,
                    square.data
                };
                for(size_t y = 0; y < 3; y++) {
                    for(size_t x = 0; x < 3; x++) {
                        auto cell = team_positions[(square.y + y - 1 + BOARD_SIZE) % BOARD_SIZE][(square.x + x - 1 + BOARD_SIZE) % BOARD_SIZE];
                        Team *other = cell.team;
                        if(!other) {
                            m.data.square.view[y][x] = 0;
                            continue;
                        }
                        if(other == this) {
                            m.data.square.view[y][x] = this->id();
                            continue;
                        }
                        TeamSquare *s = cell.square;
                        if(!s->data.stealth) {
                            m.data.square.view[y][x] = other->id();
                            continue;
                        }
                        m.data.square.view[y][x] = ((uint8_t) rand() <= s->data.stealth) ? 0 : other->id();
                    }
                }
                this->send(m);
            }

            // notify frame end with the duration the team took (ms)
            m.type = Message::FRAME_END;
            m.data.num = frame.time().count() / 1000000;
            this->send(m);
        });

        // handle receiving actions
        while(1) {
            auto r = this->receive();

            // expect an action
            // this handles the frame end ack
            if(r.type != Response::ACTION) break;

            // enqueue the action on the square
            auto act = r.data.action;
            team_positions[act.y][act.x].square->actions.push_back(std::pair<Action, Square *>(act.action, act.square));
        }

        // close the message thread
        message.join();

        // clear the board
        for(auto & square : this->squares) {
            team_positions[square.y][square.x] = {
                nullptr,
                nullptr,
            };
        }
    }

    void destroy(TeamSquare &square) {
        Message m;
        m.type = Message::DESTROY;
        m.data.destroyed.square = square.square;
        m.data.destroyed.x = square.x;
        m.data.destroyed.y = square.y;
        send(m);
        validator->destroyAt(square.x, square.y);
        square.square = nullptr;
    }
    void stillborn(Square *square) {
        Message m;
        m.type = Message::DELETE;
        m.data.destroyed.square = square;
        send(m);
    }
    void victory() {
        Message m;
        m.type = Message::VICTORY;
        send(m);
    }

    bool resolve(size_t framenum) {

        bool did_stuff = false;

        // track these separately while we iterate over this->squares
        std::queue<TeamSquare> spawned;

        // handle each of the squares
        for(auto & square : this->squares) {

            // better names for all this data
            size_t px = square.x, py = square.y;
            auto &data = square.data;

            // if the action_interval has elapsed, the square gets all its actions back
            if(!data.action_interval || framenum % data.action_interval == 0) data.current_actions = data.max_actions;

            // handle cooldown
            if(data.cooldown) {
                data.cooldown--;
                validator->activate_square(px, py, &data);
                validator->finalize_square();
                continue;
            }

            validator->activate_square(px, py, &data);

            // now handle each action
            Square *new_square = nullptr;
            if(square.actions.size()) new_square = square.actions[0].second;
            for(auto & action : square.actions) {

                // decode the action
                action_t act;
                direction_t dir;
                upgrade_t up;
                Action::decode(action.first, act, dir, up);

                // no cheating!
                if(!data.current_actions) continue;

                // get the offset position (may not be required)
                size_t x2 = px, y2 = py;
                offset(x2, y2, dir);

                if(act == action_t::NONE) continue;
                did_stuff = true;

                // handle the action
                switch(act) {

                // attack the other cell, if possible
                case action_t::ATTACK:
                    data.current_actions--;
                    if(data.can_attack) attacked[y2][x2]++;
                    validator->attack(x2, y2, data.can_attack);
                    break;

// macro to handle overflow
#define inc(x) if(!++x) x--;
                // increase stealth
                case action_t::HIDE:
                    data.current_actions--;
                    inc(data.stealth);
                    validator->hide();
                    break;

                // move to the other square
                // im gonna cave and allow some stealth to be kept (half)
                case action_t::MOVE:
                    data.current_actions--;
                    px = x2; py = y2;
                    data.stealth = data.stealth >> 1;
                    validator->move(x2, y2);
                    break;

                // make a new blank square
                case action_t::SPAWN:
                    data.current_actions = 0;
                    data.cooldown = 99;
                    // ensure valid object
                    if(new_square) {
                        TeamSquare other = spawn(new_square, x2, y2);
                        other.data.cooldown = 99;
                        spawned.push(other);
                    }
                    validator->spawn(x2, y2, new_square);
                    new_square = nullptr;
                    break;

                // clone this square
                case action_t::REPLICATE:
                    data.current_actions = 0;
                    data.cooldown = 99;
                    // ensure valid object
                    if(new_square) {
                        TeamSquare clone = square;
                        clone.square = new_square;
                        clone.x = x2;
                        clone.y = y2;
                        spawned.push(clone);
                    }
                    validator->replicate(x2, y2, new_square);
                    new_square = nullptr;
                    break;

                // upgrade this square
                case action_t::UPGRADE:
                    data.current_actions = 0;
                    data.cooldown = 99;
                    switch(up) {

                    // increase max actions and action interval
                    case upgrade_t::BLITZ:
                        inc(data.max_actions);
                        inc(data.action_interval);
                        validator->blitz();
                        break;

                    // add health but increase action interval
                    case upgrade_t::HEAVY:
                        inc(data.health);
                        inc(data.health);
                        inc(data.action_interval);
                        validator->heavy();
                        break;

                    // increase max actions but reduce health and prevent attacks
                    case upgrade_t::LIGHT:
                        if(data.health) {
                            inc(data.max_actions);
                            data.health--;
                            data.can_attack = 0;
                        }
                        validator->light();
                        break;

                    default:
                        break;
                    }

                    break;
#undef inc
                default:
                    continue;
                }
            }

            square.x = px; square.y = py;
            validator->finalize_square();

            if(new_square) stillborn(new_square);
        }

        // we've handled all actions, so clear action queues
        for(auto & square : this->squares) {
            square.actions.clear();
        }

        // add all the new squares
        while(spawned.size()) {
            this->squares.push_back(spawned.front());
            spawned.pop();
        }

        // place squares back onto board
        for(auto & square : squares) {
            LOG("placing %ld %ld (hp=%d)\n", square.x, square.y, square.data.health);
            auto & cell = team_positions[square.y][square.x];

            // empty cell, easy
            if(!cell.team) {
                cell.team = this;
                cell.square = &square;
                continue;
            }

            // another square in cell
            auto other = cell.square;
            collisions[square.y][square.x] = true;
            auto & second = second_most_health[square.y][square.x];

            // both destroyed
            if(other->data.health == square.data.health) {
                LOG("destroy both\n");
                cell.team = nullptr;
                cell.square = nullptr;
                if(square.data.health > second) second = square.data.health;
                continue;
            }

            // this destroyed
            if(other->data.health > square.data.health) {
                LOG("other stronger (other=%d, 2nd=%d)\n", other->data.health, second);
                if(square.data.health > second) second = square.data.health;

            // other destroyed
            }else {
                LOG("other weaker (other=%d, 2nd=%d)\n", other->data.health, second);
                cell.team = this;
                cell.square = &square;
                if(other->data.health > second) second = other->data.health;
            }
        }

        return did_stuff;
    }

    bool check_destroyed() {

        // iterate backwards so we can erase elements
        for(auto & square : squares) {
            auto x = square.x, y = square.y;
            LOG("checking %ld %ld (hp=%d)\n", x, y, square.data.health);
            
            auto & cell = team_positions[y][x];

            // if our square is not in the cell, we have been destroyed by collision
            if(cell.team != this || cell.square->square != square.square) {
                LOG("destroyed\n");
                destroy(square);
                continue;
            }
            
            // total damage is the number of attacks
            // + the health of the strongest square we collided with
            auto damage = attacked[y][x] + second_most_health[y][x];
            LOG("damage %d\n", damage);

            // don't check case where there is no damage or collision
            // this is to allow light squares to have 0 health
            if(!damage && !collisions[y][x]) continue;

            // square still standing, but took damage
            if(damage < square.data.health) {
                square.data.health -= damage;
                continue;

            // square destroyed, cell is now empty
            }else {
                cell.team = nullptr;
                cell.square = nullptr;
                destroy(square);
                continue;
            }
        }

        for(int64_t idx = this->squares.size()-1; idx >= 0; idx--) {
            if(this->squares[idx].square) continue;
            LOG("erasing %ld %ld\n", this->squares[idx].x, this->squares[idx].y);
            this->squares.erase(this->squares.begin()+idx);
        }

        for(auto & square : squares) {
            team_positions[square.y][square.x].square = &square;
        }

        // handle killing this team
        bool team_destroyed = !this->squares.size();
        if(team_destroyed) {
            Message m;
            m.type = Message::DEFEAT;
            send(m);
        }
        return team_destroyed;
    }
};

// publish squares to framebuffer
void draw(color_t (*framebuffer)[BOARD_SIZE], const std::vector<color_t> &colors) {
    for(size_t x = 0; x < BOARD_SIZE; x++) {
        for(size_t y = 0; y < BOARD_SIZE; y++) {
            auto & cell = team_positions[y][x];
            framebuffer[x][y] =  colors[cell.team ? cell.team->id() : 0];
        }
    }
}

// clear our work from the last round
void clear_board() {
    for(size_t x = 0; x < BOARD_SIZE; x++) {
        for(size_t y = 0; y < BOARD_SIZE; y++) {
            attacked[y][x] = 0;
            second_most_health[y][x] = 0;
            collisions[y][x] = false;
        }
    }
}

void serve(const std::vector<ProcessQueue *> &vteams, const std::vector<color_t> &vcolors, bool use_graphics, Validator *_validator) {
    
    // if no teams, do nothing
    if(!vteams.size()) return;

    if(!_validator) _validator = new Validator;
    validator = _validator;

    // make these static so they don't get destroyed
    // if this function exits from a timeout
    static Timer hard_timeout;
    static volatile bool closed = false;

    // create the graphics context and launch the frontend
    pheader_t header;
    color_t (*framebuffer)[BOARD_SIZE];
    if(use_graphics) {
        init_graphics(header, framebuffer);
        // timeout after 10s
        if(wait_frame(header, 10000)) return;
    }

    // game start
    hard_timeout.reset();
    std::thread{[&] {

        // get all the valid teams and colors
        std::list<Team> teams{};
        std::vector<color_t> colors{vcolors[0]};
        {
            std::vector<std::tuple<ProcessQueue *, Square *, int>> valid_teams;
            int teamid = 1;
            for(int i = 0; i < vteams.size(); ) {
                const auto & team = vteams[i++];
                const auto & color = vcolors[i];
                hard_timeout.reset();
                Response response;
                team->receive(response);
                if(response.type == Response::INITIAL && response.data.action.square) {
                    valid_teams.push_back(std::tuple<ProcessQueue *, Square *, int>(team, response.data.action.square, teamid++));
                    colors.push_back(color);
                }
            }

            // now that we know how many teams there are, place them evenly
            auto dims = factorize(valid_teams.size());
            size_t team_num = 0;
            size_t sx = BOARD_SIZE / dims.first, sy = BOARD_SIZE / dims.second;
            for(size_t x = 0; x < dims.first; x++) {
                for(size_t y = 0; y < dims.second; y++) {
                    auto team = valid_teams[team_num++];
                    size_t px = sx / 2, py = sy / 2;
                    if(py > 4) {
                        px = 2 + (size_t) rand() % (sx - 4);
                        py = 2 + (size_t) rand() % (sy - 4);
                    }
                    size_t startx = sx*x + px, starty = sy*y + py;
                    teams.push_back(Team(std::get<2>(team), std::get<0>(team), std::get<1>(team), startx, starty));
                    validator->start(startx, starty);
                }
            }
        }

        // draw the first frame with all the teams placed
        size_t framenum = 0;
        if(use_graphics) {
            draw(framebuffer, colors);
            publish_frame(header);
            if(wait_frame(header, 1000)) goto end;
        }

        while(1) {
            // frame start, reset timeout
            hard_timeout.reset();
            validator->frame(framenum);

            // start getting actions from teams
            LOG("getting actions\n");
            std::vector<std::thread> team_threads;
            for(auto & team : teams) {
                team_threads.push_back(std::thread([&team] {
                    team.get_actions();
                }));
            }

            // finish getting actions
            for(auto & th : team_threads) {
                th.join();
            }
            
            // this is all my code from here, so no need to worry about timeout
            hard_timeout.reset();

            // now actually perform the actions
            {
                LOG("resolve actions\n");
                bool did_stuff = false;
                for(auto & team : teams) {
                    did_stuff = team.resolve(framenum) || did_stuff;
                }
                validator->finish_frame();
                framenum++;
                if(!did_stuff) continue;
            }
            LOG("check destroyed\n");
            size_t prev_alive = teams.size();
            int num_alive = 0;
            for(auto it = teams.begin(); it != teams.end(); ) {
                if(it->check_destroyed()) {
                    it = teams.erase(it);
                }else {
                    it++;
                    num_alive++;
                }
            }

            // render
            if(use_graphics) {
                draw(framebuffer, colors);
                publish_frame(header);
                if(wait_frame(header, 1000)) break;
            }

            // clear the data from the last round
            clear_board();

            // exit when 1 or fewer teams remain
            if(num_alive == 0) break;
            if(num_alive == 1 && prev_alive != 1) {
                teams.begin()->victory();
                break;
            }
        }

        end:
        closed = true;
    }}.detach();
    
    // yield until the game ends or a team hangs
    while(!closed && !hard_timeout.timeout(5000)) std::this_thread::yield();

    // kill the graphics process, if any
    if(use_graphics) close_graphics();

    delete _validator;
}
