
#include <dlfcn.h>
#include <unistd.h>

#include <iostream>

#define IMPORT
#include "interface.h"
#undef IMPORT

#include "team_runner.h"

#define printf(...) printf(__VA_ARGS__); fflush(stdout)
#define load(func) func = (decltype(func)) dlsym(handle, #func); {\
    auto err = dlerror();\
    if(err) printf("%s\n", err);\
}

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
    load(display_name);
    load(start);
    load(victory);
    load(defeat);

    printf("%p %p %p %p\n", display_name, start, victory, defeat);

    // assert that we loaded all the symbols
    if(!(display_name && start && victory && defeat)) {
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
    response.ACTION;
    response.data.action.square = start(team);
    queue.send(response);
    
    auto square = start(1);
    int view[3][3];
    size_t pos[2];
    SquareData data;
    Square *spawn;
    for(const auto & act : square->act(view, pos, data, spawn)) {
        action_t a;
        direction_t d;
        upgrade_t u;
        Action::decode(act, a, d, u);
        printf("%d %d %d\n", a, d, u);
    }
    delete square;
    printf("%s\n%s\n", victory(), defeat());

    dlclose(handle);
}
