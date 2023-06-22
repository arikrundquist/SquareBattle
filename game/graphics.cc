
#include "graphics.h"
#include "../utils/Timer.h"

#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>

/**
 * here we use compile-time conditionals to increase efficiency when RESOLUTION is 1
 * 
 * if RESOLUTION is not 1, we must use an additional buffer to upsample the framebuffer,
 * but when RESOLUTION is 1 we can just work directly with the buffer
 */

color_t (*_frame_)[BOARD_SIZE*RESOLUTION] = nullptr;
#if RESOLUTION != 1
color_t board[BOARD_SIZE][BOARD_SIZE] = {0};
#endif

pid_t graphics_pid = 0;

void init_graphics(pheader_t &header , color_t (*&framebuffer)[BOARD_SIZE]) {

    // create a memory mapped file for the framebuffer
#define GRAPHICS_SIZE BOARD_SIZE*BOARD_SIZE*RESOLUTION*RESOLUTION*3+sizeof(header_t)
    int fd = open("game.graphics", O_CREAT|O_RDWR);
    ftruncate(fd, GRAPHICS_SIZE);
    char *addr = (char *) mmap(0, GRAPHICS_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
#undef GRAPHICS_SIZE

    // first part of the file will have meta data
    header = (pheader_t) addr;
    header->frame_ticker = false;
    header->framebuffer_size = BOARD_SIZE*RESOLUTION;

    // rest of the file is the pixel data
    _frame_ = (color_t (*)[BOARD_SIZE*RESOLUTION]) &addr[sizeof(header_t)];

    // framebuffer is either directly mapped to the file, or requires an additional buffer
#if RESOLUTION == 1
    framebuffer = _frame_;
    for(int y = 0; y < BOARD_SIZE; y++) for(int x = 0; x < BOARD_SIZE; x++) framebuffer[y][x] = {0, 0, 0};
#else
    framebuffer = board;
#endif

    // spawn the frontend
    graphics_pid = fork();
    if(!graphics_pid) {
        int e = execlp("/root/game/graphics.py", 0);

        // this should never be reached
        std::cout << "error: " << errno << std::endl;
    }
}

void publish_frame(pheader_t header) {
    // if necessary, copy data from buffer into shared framebuffer
#if RESOLUTION != 1
    for(int y = 0; y < BOARD_SIZE*RESOLUTION; y++) for(int x = 0; x < BOARD_SIZE*RESOLUTION; x++) _frame_[y][x] = board[y/RESOLUTION][x/RESOLUTION];
#endif
    header->frame_ticker = false;
}
bool wait_frame(pheader_t header, int timeout) {
    Timer timer;

    // wait for reponse from python saying frame was updated
    while(!timer.timeout(timeout) && !header->frame_ticker) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !header->frame_ticker;
}

void close_graphics() {

    // force kill python
    kill(graphics_pid, SIGKILL);
}

// for debug, fills the framebuffer with new pixel data each frame until graphics is closed
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
