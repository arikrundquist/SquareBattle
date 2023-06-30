
#include <dlfcn.h>
#include <unistd.h>

#include <iostream>

#define IMPORT
#include "interface.h"
#undef IMPORT

#include "team_runner.h"

#define printf(...) printf(__VA_ARGS__); fflush(stdout)
#define load(func) func = (decltype(func)) dlsym(handle, #func)

void launch_team(ProcessQueue &queue, int team) {

    Message message;
    Response response;

    // open the team's shared library
    auto handle = dlopen("./team.so", RTLD_NOW|RTLD_GLOBAL);
    if(!handle) {
        printf("%s\n", dlerror());
        printf("failed to load team: %d\n", team);
        response.type = Response::INVALID;
        queue.send(response);
        return;
    }

    // load symbols from team
    load(start);

    // these are optional
    load(display_name);
    load(victory);
    load(defeat);

    // assert that we loaded a start function
    if(!start) {
        if(display_name) {
            printf("failed to load team: %s\n", display_name());
        }else {
            printf("failed to load team: %d\n", team);
        }
        response.type = Response::INVALID;
        queue.send(response);
        dlclose(handle);
        return;
    }

    // send the starting object to the backend
    response.type = Response::INITIAL;
    response.data.action.square = start(team);
    queue.send(response);
    
    while(1) {
        queue.receive(message);

        // handle square destruction
        if(message.type == Message::DESTROY) {
            size_t pos[2];
            auto square = message.data.destroyed;
            pos[0] = square.x; pos[1] = square.y;
            
            // notify player of square death
            square.square->destroyed(pos);

            // player should not delete the square
            delete square.square;
        }

        // expect a frame start
        if(message.type != Message::FRAME_START) break;
        response.type = Response::ACTION;
        while(1) {
            queue.receive(message);

            // expect request for square to determine action
            // this also catches the frame end message
            if(message.type != Message::SQUARE) break;

            // get action(s) from square
            auto square = message.data.square.data;
            size_t pos[2];
            pos[0] = square.x; pos[1] = square.y;
            Square *spawn = nullptr;
            auto acts = square.square->act(message.data.square.view, pos, square.data, spawn);
            response.data.action.square = spawn;
            response.data.action.x = square.x;
            response.data.action.y = square.y;

            // send each action individually
            for(const auto & act : acts) {
                response.data.action.action = act;
                queue.send(response);
            }
        }

        // send this as a flag that we have stopped sending actions
        response.type = Response::INVALID;
        queue.send(response);
    }

    dlclose(handle);
}
