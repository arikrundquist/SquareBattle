
#include <dlfcn.h>
#include <unistd.h>

#include <iostream>

#define IMPORT
#include "../api/square.h"
#undef IMPORT

#include "team_runner.h"

#define printf(...) printf(__VA_ARGS__); fflush(stdout)
#define load(func) func = (decltype(func)) dlsym(handle, #func);

void launch_team(const ProcessQueue &queue) {

    auto handle = dlopen("./team.so", RTLD_LAZY);

    printf("%p\n", handle);

    load(start);
    load(victory);
    load(defeat);
    printf("%p %p %p\n", start, victory, defeat);
    auto square = start(1);
    int view[3][3];
    int pos[2];
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
    printf("%s\n", dlerror());

    dlclose(handle);
}
