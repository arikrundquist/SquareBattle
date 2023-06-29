
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <filesystem>
#include <sys/prctl.h>
#include <signal.h>
#include <time.h>
#include <queue>

#include "team_runner.h"
#include "server.h"

// TODO make this not just a void *
ProcessQueue &launch_team(const std::string &team_dir, int team, pid_t &pid) {
    
    // create a region of shared memory with the child process and place a message queue in it
    void *addr = mmap(0, sizeof(ProcessQueue), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    auto &queue = *(new(addr) ProcessQueue);

    // create the child process, return the shared memory
    pid = fork();
    if(pid) return queue;

    /* ensure teams cannot cheat by looking at other teams */
    const char *path = team_dir.c_str();
    // set the root directory to the team directory
    chroot(path);
    // chroot does not change the working directory (odd, but okay)
    chdir("/");
    // drop all privileges
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
    // this should do nothing after dropping privileges
    chroot("/");
    /* now, all teams appear to load at the root directory */
    
    // TODO load a dll from these locations and start running
    launch_team(queue, team);

    // we should never reach this once everything is implemented
    exit(-1);
}

// simple routine to read a number from a string
template <typename T>
T read(char *num) {
    T val = 0;
    for(int i = 0; num[i]; i++) {
        val = val * 10 + num[i] - '0';
    }
    return val;
}

int main(int argc, char **argv) {

    // we need to load teams and colors as arguments
    std::queue<std::string> team_dirs{};
    std::vector<color_t> team_colors{};

    // default background is white
    team_colors.push_back({255, 255, 255});

    // handle args
    for(int i = 1; i < argc; i++) {
        char *arg = argv[i];

        // expect a flag to label the args
        if(arg[0] != '-') continue;

        // -b r g b
        // set the background color
        if(arg[1] == 'b') {
            team_colors[0] = {
                read<uint8_t>(argv[++i]),
                read<uint8_t>(argv[++i]),
                read<uint8_t>(argv[++i]),
            };
        }

        // -t team r g b
        // add a team by directory with a specific color
        if(arg[1] == 't') {
            team_dirs.push(std::string(argv[++i]));
            team_colors.push_back({
                read<uint8_t>(argv[++i]),
                read<uint8_t>(argv[++i]),
                read<uint8_t>(argv[++i]),
            });
        }

        // -s seed
        // set the seed
        if(arg[1] == 's') {
            size_t seed = read<size_t>(argv[++i]);
            if(!seed) seed = time(0);
            srand(seed);
            printf("seed: %ld\n", seed);
            fflush(stdout);
        }
    }

    // launch processes for each team
    std::vector<pid_t> pids{};
    std::vector<ProcessQueue *> teams{};
    while(team_dirs.size()) {
        pid_t pid = 0;
        teams.push_back(&launch_team(team_dirs.front(), teams.size() + 1, pid));
        pids.push_back(pid);
        team_dirs.pop();
    }

    // run the game
    serve(teams, team_colors);

    // kill all child processes
    end:
    for(const auto &pid : pids) {
        kill(pid, SIGKILL);
    }
    close_graphics();

    return 0;
}
