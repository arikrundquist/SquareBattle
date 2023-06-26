
#include <thread>
#include <list>
#include <queue>
#include <map>

#include "server.h"
#include "../utils/Timer.h"

struct TeamSquare {
    Square *square;
    SquareData data;
    std::queue<Action> actions;
};

class Team {
    std::unordered_map<size_t, std::unordered_map<size_t, TeamSquare>> squares;
public:
    Team(ProcessQueue *queue, Square *square) {
        size_t x = rand(), y = rand();

        TeamSquare first = {
            square,
            {
                1,
                1,
                1,
                0,
                0,
                0,
                0,
                1
            },
            std::queue<Action>{}
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
                framebuffer[y.first][x.first] = {0xFF, 0xFF, 0xFF};
            }
        }
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

        std::list<Team> teams{};
        for(const auto & team : vteams) {
            Response response;
            team->receive(response);
            if(response.type == Response::ACTION && response.data.action.square) {
                teams.push_back(Team(team, response.data.action.square));
            }
        }

        for(const auto & team : teams) {
            team.draw({0, 0, 0}, framebuffer);
        }

        while(1) {
            // frame start, reset timeout
            hard_timeout.reset();

            //for()

            // render
            publish_frame(header);
            if(wait_frame(header, 200)) break;
        }
        closed = true;
    }}.detach();
    
    // yield until the game ends or a team hangs
    while(!closed && !hard_timeout.timeout(5000)) std::this_thread::yield();
}
