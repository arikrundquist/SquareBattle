
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <filesystem>
#include <sys/prctl.h>
#include <signal.h>

#include "graphics.h"
#include "team_runner.h"

// TODO make this not just a void *
ProcessQueue &launch_team(const std::string &team_dir, pid_t &pid) {
    
    // create a region of shared memory with the child process and place a message queue in it
    void *addr = mmap(0, sizeof(ProcessQueue), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    new(addr) ProcessQueue;
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
    launch_team(queue);

    // we should never reach this once everything is implemented
    exit(-1);
}

int main() {

    // eventually, this will launch processes for each team
    std::vector<pid_t> pids{};
    for (const auto & entry : std::filesystem::directory_iterator("teams")) {
        if(!entry.is_directory()) continue;
        pid_t pid = 0;
        launch_team(entry.path().string(), pid);
        pids.push_back(pid);
    }

    // create the graphics context and launch the frontend
    pheader_t header;
    color_t (*framebuffer)[BOARD_SIZE];
    init_graphics(header, framebuffer);
    // timeout after 10s
    if(wait_frame(header, 10000)) goto end;
    
    // TODO replace with the actual game
    test_graphics(header, framebuffer);

    // kill all child processes
    end:
    for(const auto &pid : pids) {
        kill(pid, SIGKILL);
    }
    close_graphics();

    return 0;
}
