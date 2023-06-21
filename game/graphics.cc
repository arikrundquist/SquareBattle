
#include "graphics.h"
#include "../utils/Timer.h"
#include <signal.h>

color_t (*_frame_)[BOARD_SIZE*RESOLUTION] = nullptr;
#if RESOLUTION != 1
color_t board[BOARD_SIZE][BOARD_SIZE] = {0};
#endif

pid_t graphics_pid = 0;

void init_graphics(pheader_t &header , color_t (*&framebuffer)[BOARD_SIZE]) {
    int fd = open("game.graphics", O_CREAT|O_RDWR);
#define GRAPHICS_SIZE BOARD_SIZE*BOARD_SIZE*RESOLUTION*RESOLUTION*3+sizeof(header_t)
    ftruncate(fd, GRAPHICS_SIZE);
    char *addr = (char *) mmap(0, GRAPHICS_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
#undef GRAPHICS_SIZE
    close(fd);
    header = (pheader_t) addr;
    header->frame_ticker = false;
    header->framebuffer_size = BOARD_SIZE*RESOLUTION;
    _frame_ = (color_t (*)[BOARD_SIZE*RESOLUTION]) &addr[sizeof(header_t)];
#if RESOLUTION == 1
    framebuffer = _frame_;
    for(int y = 0; y < BOARD_SIZE; y++) for(int x = 0; x < BOARD_SIZE; x++) framebuffer[y][x] = {0, 0, 0};
#else
    framebuffer = board;
#endif

    graphics_pid = fork();
    if(!graphics_pid) {
        int e = execlp("/root/game/graphics.py", 0);
        std::cout << "error: " << errno << std::endl;
    }
}

void publish_frame(pheader_t header) {
#if RESOLUTION != 1
    for(int y = 0; y < BOARD_SIZE*RESOLUTION; y++) for(int x = 0; x < BOARD_SIZE*RESOLUTION; x++) _frame_[y][x] = board[y/RESOLUTION][x/RESOLUTION];
#endif
    header->frame_ticker = false;
}
bool wait_frame(pheader_t header, int timeout) {
    Timer timer;
    while(!timer.timeout(timeout) && !header->frame_ticker) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !header->frame_ticker;
}

void test_graphics(pheader_t header, color_t (*framebuffer)[BOARD_SIZE]) {
    for(int i = 0; ; i++) {
        for(int y = 0; y < BOARD_SIZE; y++) {
            for(int x = 0; x < BOARD_SIZE; x++) {
                framebuffer[y][x] = {(uint8_t) x, (uint8_t) (i + rand()), (uint8_t) y};
            }
        }
        publish_frame(header);
        if(wait_frame(header, 1000)) break;
    }
    std::cout << "no frame after 1s, closing" << std::endl;
}
