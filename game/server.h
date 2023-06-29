
#ifndef SERVER_H
#define SERVER_H

#include <vector>

#include "backend.h"
#include "graphics.h"

void serve(const std::vector<ProcessQueue *> &teams, const std::vector<color_t> &colors);

#endif
