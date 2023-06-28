
#include <thread>
#include <list>
#include <deque>
#include <map>
#include <tuple>

#include "server.h"
#include "../utils/Timer.h"

#include <iostream>
#define printf(...) printf(__VA_ARGS__); fflush(stdout)

class Team;
Team *team_positions[BOARD_SIZE][BOARD_SIZE] = {0};
uint16_t attacked[BOARD_SIZE][BOARD_SIZE] = {0};

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
    std::deque<std::pair<Action, Square *>> actions;
};

class Team {
    const int teamid;
    ProcessQueue *queue;
    std::unordered_map<size_t, std::unordered_map<size_t, TeamSquare>> squares;
public:
    Team(int teamid, ProcessQueue *queue, Square *square, size_t x, size_t y) : teamid(teamid), queue(queue) {
        TeamSquare first = {
            square,
            {
                0,
                1,
                1,
                1,
                0,
                0,
                1
            },
            std::deque<std::pair<Action, Square *>>{}
        };
        set(x % BOARD_SIZE, y % BOARD_SIZE, first);
    }
    inline void set(size_t x, size_t y, const TeamSquare &square) {
        if(!squares.count(x)) squares[x] = std::unordered_map<size_t, TeamSquare>{};
        squares[x][y] = square;
    }
    inline void remove(size_t x, size_t y) {
        squares[x].erase(y);
        if(!squares[x].size()) squares.erase(x);
    }
    void draw(color_t color, color_t (*framebuffer)[BOARD_SIZE]) const {
        for(const auto & x : this->squares) {
            for(const auto & y : x.second) {
                framebuffer[x.first][y.first] = color;
            }
        }
    }
    inline void send(const Message &message) {
        this->queue->send(message);
    }
    inline Response receive() {
        Response response;
        this->queue->receive(response);
        return response;
    }

    void get_actions() {
        auto message = std::thread([this] {
            Timer frame;
            Message m;
            m.type = Message::FRAME_START;
            this->send(m);
            m.type = Message::SQUARE;
            for(const auto & x : this->squares) {
                for(const auto & y : x.second) {
                    auto square = y.second;
                    if(square.data.cooldown) {
                        square.data.cooldown--;
                        continue;
                    }
                    m.data.square = {
                        square.square,
                        x.first,
                        y.first,
                        square.data
                    };
                    this->send(m);
                }
            }
            m.type = Message::FRAME_END;
            m.data.num = frame.time().count() / 1000000;
            this->send(m);
        });
        while(1) {
            auto r = this->receive();
            if(r.type != Response::ACTION) break;
            auto act = r.data.action;
            this->squares[act.x][act.y].actions.push_back(std::pair<Action, Square *>(act.action, act.square));
        }
        message.join();
    }

    void resolve(size_t framenum) {
        for(const auto & x : this->squares) {
            for(const auto & y : x.second) {
                size_t px = x.first, py = y.first;
                auto &square = this->squares[px][py];
                auto &data = square.data;
                if(framenum & data.action_interval == data.action_interval) data.current_actions = data.max_actions;
                if(data.cooldown) {
                    if(data.cooldown >= data.current_actions) {
                        data.cooldown -= data.current_actions;
                        data.current_actions = 0;
                        continue;
                    }else {
                        data.current_actions -= data.cooldown;
                        data.cooldown = 0;
                    }
                }
                for(auto & action : square.actions) {
                    if(!data.current_actions) break;
                    action_t act;
                    direction_t dir;
                    upgrade_t up;
                    Action::decode(action.first, act, dir, up);
                    size_t x2 = px, y2 = py;
                    offset(x2, y2, dir);
                    switch(act) {
                    case action_t::ATTACK:
                        data.current_actions--;
                        if(data.can_attack) attacked[y2][x2]++;
                        break;
                    case action_t::HIDE:
                        data.current_actions--;
                        data.stealth++;
                        break;
                    case action_t::MOVE:
                        data.current_actions--;
                        px = x2; py = y2;
                        // TODO
                        break;
#define COOLDOWN(x)     if(data.current_actions >= x) {\
                            data.current_actions -= x;\
                        }else {\
                            data.cooldown = x - data.current_actions;\
                            data.current_actions = 0;\
                        }
                    case action_t::REPLICATE: COOLDOWN(100)
                        // TODO
                        break;
                    case action_t::SPAWN: COOLDOWN(1000)
                        // TODO
                        break;
                    case action_t::UPGRADE: COOLDOWN(100)
                        switch(up) {
                        case upgrade_t::BLITZ:
                            data.max_actions++;
                            data.action_interval++;
                            break;
                        case upgrade_t::HEAVY:
                            data.health++;
                            data.action_interval++;
                            break;
                        case upgrade_t::LIGHT:
                            if(!data.health) break;
                            data.max_actions++;
                            data.health--;
                            break;
                        default:
                            break;
                        }
                        break;
#undef COOLDOWN
                    default:
                        continue;
                    }
                }
            }
        }
    }

    void check_destroyed() {
        // TODO
    }
};

void serve(const std::vector<ProcessQueue *> &vteams) {
    
    // make these static so they don't get destroyed
    // if this function exits from a timeout
    static Timer hard_timeout;
    static volatile bool closed = false;

    // create the graphics context and launch the frontend
    pheader_t header;
    color_t (*framebuffer)[BOARD_SIZE];
    init_graphics(header, framebuffer);
    // timeout after 10s
    if(wait_frame(header, 10000)) return;

    // game start
    hard_timeout.reset();
    std::thread{[&] {

        // get all the valid teams
        std::list<Team> teams{};
        {
            std::vector<std::tuple<ProcessQueue *, Square *, int>> valid_teams;
            int teamid = 1;
            for(const auto & team : vteams) {
                hard_timeout.reset();
                Response response;
                team->receive(response);
                if(response.type == Response::INITIAL && response.data.action.square) {
                    valid_teams.push_back(std::tuple<ProcessQueue *, Square *, int>(team, response.data.action.square, teamid++));
                }
            }

            // now that we know how many teams there are, place them evenly
            auto dims = factorize(valid_teams.size());
            size_t team_num = 0;
            size_t sx = BOARD_SIZE / dims.first, sy = BOARD_SIZE / dims.second;
            for(size_t x = 0; x < dims.first; x++) {
                for(size_t y = 0; y < dims.second; y++) {
                    auto team = valid_teams[team_num++];
                    teams.push_back(Team(std::get<2>(team), std::get<0>(team), std::get<1>(team), sx*x + sx/2, sy*y + sy/2));
                }
            }
        }

        // draw the first frame with all the teams placed
        size_t framenum = 0;
        for(const auto & team : teams) {
            team.draw({0, 0, 0}, framebuffer);
        }
        publish_frame(header);
        if(wait_frame(header, 200)) goto end;

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

            // clear the board
            // TODO actually be smart about this
            for(int i = 0; i < BOARD_SIZE; i++) {
                for(int j = 0; j < BOARD_SIZE; j++) {
                    team_positions[i][j] = 0;
                    attacked[i][j] = 0;
                }
            }

            // now actually perform the actions
            for(auto & team : teams) {
                team.resolve(framenum);
            }
            for(auto & team : teams) {
                team.check_destroyed();
            }
            for(auto & team : teams) {
                team.draw({0, 0, 0}, framebuffer);
            }

            // render
            publish_frame(header);
            if(wait_frame(header, 200)) break;
            framenum++;
        }

        end:
        closed = true;
    }}.detach();
    
    // yield until the game ends or a team hangs
    while(!closed && !hard_timeout.timeout(5000)) std::this_thread::yield();
}
