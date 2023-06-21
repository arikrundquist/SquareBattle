
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <unistd.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <filesystem>
#include <thread>
#include <sys/mman.h>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <chrono>

#define BOARD_SIZE (1 << 6)
#define RESOLUTION 1

typedef struct {
    uint8_t r, g, b;
} color_t;

typedef struct {
    volatile bool frame_ticker;
    uint32_t framebuffer_size;
} header_t, *pheader_t;

void init_graphics(pheader_t &header , color_t (*&framebuffer)[BOARD_SIZE]);
void publish_frame(pheader_t header);
bool wait_frame(pheader_t header, int timeout);

void test_graphics(pheader_t header, color_t (*framebuffer)[BOARD_SIZE]);

#endif
