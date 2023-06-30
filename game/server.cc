
#include <thread>
#include <list>
#include <deque>
#include <queue>
#include <tuple>

#include "server.h"
#include "../utils/Timer.h"

#include <iostream>
#define printf(...) printf(__VA_ARGS__); fflush(stdout)
#define DEBUG printf("%s %d\n", __FUNCTION__, __LINE__);

class Team;
struct TeamSquare;
struct {
    Team *team;
    TeamSquare *square;
} team_positions[BOARD_SIZE][BOARD_SIZE] = {0};
uint16_t attacked[BOARD_SIZE][BOARD_SIZE] = {0};
uint8_t second_most_health[BOARD_SIZE][BOARD_SIZE] = {0};

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
    inline int id() { return teamid; }
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
                if(!square.data.current_actions) continue;

                // send the data
                m.data.square.data = {
                    square.square,
                    square.x,
                    square.y,
                    square.data
                };
                for(size_t y = 0; y < 3; y++) {
                    for(size_t x = 0; x < 3; x++) {
                        Team *other = team_positions[(square.y + y - 1 + BOARD_SIZE) % BOARD_SIZE][(square.x + x - 1 + BOARD_SIZE) % BOARD_SIZE].team;
                        m.data.square.view[y][x] = other ? other->id() : 0;
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
    }

    void destroy(TeamSquare &square) {
        Message m;
        m.type = Message::DESTROY;
        m.data.destroyed.square = square.square;
        m.data.destroyed.x = square.x;
        m.data.destroyed.y = square.y;
        send(m);
    }

    void resolve(size_t framenum) {

        // track these separately while we iterate over this->squares
        std::queue<TeamSquare> spawned;

        // handle each of the squares
        for(auto & square : this->squares) {

            // better names for all this data
            size_t px = square.x, py = square.y;
            auto &data = square.data;

            // clear the board
            team_positions[py][px] = {
                nullptr,
                nullptr,
            };

            // if the action_interval has elapsed, the square gets all its actions back
            if(framenum & data.action_interval == data.action_interval) data.current_actions = data.max_actions;

            // handle cooldown
            if(data.cooldown) {

                // can apply all actions to cooldown
                if(data.cooldown >= data.current_actions) {
                    data.cooldown -= data.current_actions;
                    data.current_actions = 0;
                    continue;
                
                // can pay for cooldown with current actions
                }else {
                    data.current_actions -= data.cooldown;
                    data.cooldown = 0;
                }
            }

            // now handle each action
            for(auto & action : square.actions) {

                // no cheating!
                if(!data.current_actions) break;

                // decode the action
                action_t act;
                direction_t dir;
                upgrade_t up;
                Action::decode(action.first, act, dir, up);

                // get the offset position (may not be required)
                size_t x2 = px, y2 = py;
                offset(x2, y2, dir);

                // handle the action
                switch(act) {

                // attack the other cell, if possible
                case action_t::ATTACK:
                    data.current_actions--;
                    if(data.can_attack) attacked[y2][x2]++;
                    break;

// macro to handle overflow
#define inc(x) if(!++x) x--;
                // increase stealth
                case action_t::HIDE:
                    data.current_actions--;
                    inc(data.stealth);
                    break;

                // move to the other square
                // im gonna cave and allow some stealth to be kept (half)
                case action_t::MOVE:
                    data.current_actions--;
                    px = x2; py = y2;
                    data.stealth = data.stealth >> 1;
                    // TODO
                    break;

// quick macro to add a cooldown
#define COOLDOWN(x)     if(data.current_actions >= x) {\
                        data.current_actions -= x;\
                    }else {\
                        data.cooldown = x - data.current_actions;\
                        data.current_actions = 0;\
                    }

                // make a new blank square
                case action_t::SPAWN: COOLDOWN(100)
                    // ensure valid object
                    if(action.second) {
                        TeamSquare other = spawn(action.second, x2, y2);
                        other.data.cooldown = 100;
                        spawned.push(other);
                    }
                    break;

                // clone this square
                case action_t::REPLICATE: COOLDOWN(1000)
                    // ensure valid object
                    if(action.second) {;
                        TeamSquare clone = square;
                        clone.square = action.second;
                        clone.x = x2;
                        clone.y = y2;
                        // intentionally not overwriting clone.data.cooldown
                        spawned.push(clone);
                    }
                    break;

                // upgrade this square
                case action_t::UPGRADE: COOLDOWN(100)
                    switch(up) {

                    // increase max actions and action interval
                    case upgrade_t::BLITZ:
                        inc(data.max_actions);
                        inc(data.action_interval);
                        break;

                    // add health but increase action interval
                    case upgrade_t::HEAVY:
                        inc(data.health);
                        inc(data.action_interval);
                        break;

                    // increase max actions but reduce health and prevent attacks
                    case upgrade_t::LIGHT:
                        if(!data.health) break;
                        inc(data.max_actions);
                        data.health--;
                        data.can_attack = 0;
                        break;

                    default:
                        break;
                    }
                    break;
#undef COOLDOWN
#undef inc
                default:
                    continue;
                }
            }

            square.x = px; square.y = py;
        }

        // add all the new squares
        while(spawned.size()) {
            this->squares.push_back(spawned.front());
            spawned.pop();
        }

        // place squares back onto board
        for(auto & square : squares) {
            auto & cell = team_positions[square.y][square.x];

            // empty cell, easy
            if(!cell.team) {
                cell.team = this;
                cell.square = &square;
                continue;
            }

            // another square in cell
            auto other = cell.square;

            // both destroyed
            if(other->data.health == square.data.health) {
                cell.team = nullptr;
                cell.square = nullptr;
                second_most_health[square.y][square.x] = square.data.health;
                return;
            }

            // this destroyed
            if(other->data.health > square.data.health) {
                auto & second = second_most_health[square.y][square.x];
                if(square.data.health > second) second = square.data.health;

            // other destroyed
            }else {
                auto & second = second_most_health[square.y][square.x];
                if(other->data.health > second) second = other->data.health;
            }
        }
    }

    bool check_destroyed() {

        // iterate backwards so we can erase elements
        for(int64_t i = squares.size()-1; i >= 0; i--) {
            auto & square = squares[i];
            auto x = square.x, y = square.y;
            
            auto & cell = team_positions[y][x];
            
            // if our square is not in the cell, we have been destroyed by collision
            if(cell.team != this || cell.square->square != square.square) {
                destroy(square);
                squares.erase(squares.begin() + i);
                continue;
            }
            
            // total damage is the number of attacks
            // + the health of the strongest square we collided with
            auto damage = attacked[y][x] + second_most_health[y][x];
            
            // don't check case where there is no damage
            // this is to allow light squares to have 0 health
            if(!damage) continue;

            // square still standing, but took damage
            if(damage < square.data.health) {
                square.data.health -= damage;
                continue;

            // square destroyed, cell is now empty
            }else {
                cell.team = nullptr;
                cell.square = nullptr;
                destroy(square);
                squares.erase(squares.begin() + i);
                continue;
            }
        }

        // handle killing this team
        bool team_destroyed = this->squares.size();
        if(team_destroyed) {
            Message m;
            m.type = Message::CLOSE;
            send(m);
        }
        return team_destroyed;
    }
};

void draw(color_t (*framebuffer)[BOARD_SIZE], const std::vector<color_t> &colors) {
    for(size_t x = 0; x < BOARD_SIZE; x++) {
        for(size_t y = 0; y < BOARD_SIZE; y++) {
            auto & cell = team_positions[y][x];
            if(cell.team) framebuffer[x][y] = colors[cell.team->id()];
            else framebuffer[x][y] = colors[0];
            
            // clear the board
            attacked[y][x] = 0;
            second_most_health[y][x] = 0;
        }
    }
}

void serve(const std::vector<ProcessQueue *> &vteams, const std::vector<color_t> &vcolors, bool use_graphics) {
    
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
        std::vector<color_t> colors{};
        {
            std::vector<std::tuple<ProcessQueue *, Square *, int>> valid_teams;
            int teamid = 1;
            for(int i = 0; i < vteams.size(); i++) {
                const auto & team = vteams[i];
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
                    teams.push_back(Team(std::get<2>(team), std::get<0>(team), std::get<1>(team), sx*x + px, sy*y + py));
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

            // start getting actions from teams
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
            for(auto & team : teams) {
                team.resolve(framenum);
            }
            int num_alive = 0;
            for(auto & team : teams) {
                if(team.check_destroyed()) num_alive++;
            }

            // render
            if(use_graphics) {
                draw(framebuffer, colors);
                publish_frame(header);
                if(wait_frame(header, 1000)) break;
                framenum++;
            }

            // if we are running headless, exit when 1 or fewer teams remain
            if(!use_graphics && num_alive < 2) break;
        }

        end:
        closed = true;
    }}.detach();
    
    // yield until the game ends or a team hangs
    while(!closed && !hard_timeout.timeout(5000)) std::this_thread::yield();
}
