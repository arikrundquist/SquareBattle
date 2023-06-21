
#include "graphics.h"
#include "../api/api.h"
#include "../utils/ProcessQueue.h"
#include <pwd.h>
#include <sys/errno.h>
#include <sys/prctl.h>

// TODO clean this up
void *fork_dir(const std::string &str) {
    void *addr = mmap(0, BOARD_SIZE*BOARD_SIZE*sizeof(Action::NONE), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    std::string copy = str;
    const char *path = copy.c_str();
    if(fork()) return addr;
    
    // TODO make this a process queue
    auto s = new(addr) MessageQueue<BOARD_SIZE*BOARD_SIZE>;
    std::cout << path << std::endl;
    chroot(path);
    chdir("/");
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
    chroot("/");
    for(const auto & entry : std::filesystem::directory_iterator(".")) {
        std::cout << entry.path().string() << std::endl;
    }
    char cwd[100];
    getcwd(cwd, 100);
    std::cout << cwd << std::endl;
    // TODO load a dll from these locations
    exit(0);
}
// kill(pid, SIGKILL);
//int sem_init(sem_t *sem, int pshared, unsigned int value);

int main() {

    // eventually, this will launch processes for each team
    for (const auto & entry : std::filesystem::directory_iterator("teams")) {
        if(!entry.is_directory()) continue;
        fork_dir(entry.path().string());
        wait(NULL);
    }

    pheader_t header;
    color_t (*framebuffer)[BOARD_SIZE];
    init_graphics(header, framebuffer);
    wait_frame(header, 10000);
    
    test_graphics(header, framebuffer);

    return 0;
}
