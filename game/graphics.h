
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <cstdint>

#include "board_size.h"

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
void close_graphics();

void test_graphics(pheader_t header, color_t (*framebuffer)[BOARD_SIZE]);

#endif
