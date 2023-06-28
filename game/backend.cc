
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <filesystem>
#include <sys/prctl.h>
#include <signal.h>
#include <time.h>

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

int main() {

    // set the seed
    // eventually, this may be used to repeat runs
    size_t seed = time(0);
    srand(seed);
    printf("seed: %ld\n", seed);
    fflush(stdout);

    // launch processes for each team
    std::vector<pid_t> pids{};
    std::vector<ProcessQueue *> teams{};
    for(int copies = 0; copies < 9; copies++)
    for (const auto & entry : std::filesystem::directory_iterator("teams")) {
        if(!entry.is_directory()) continue;
        pid_t pid = 0;
        teams.push_back(&launch_team(entry.path().string(), teams.size() + 1, pid));
        pids.push_back(pid);
    }

    // run the game
    serve(teams);

    // kill all child processes
    end:
    for(const auto &pid : pids) {
        kill(pid, SIGKILL);
    }
    close_graphics();

    return 0;
}
