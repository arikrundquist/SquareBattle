
#include "../utils/ProcessQueue.h"

#ifndef BACKEND_H
#define BACKEND_H

typedef InterprocessQueues<1024, uint32_t, uint64_t> ProcessQueue;

#endif
