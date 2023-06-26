
#include "../utils/ProcessQueue.h"
#include "interface.h"

#ifndef BACKEND_H
#define BACKEND_H

typedef InterprocessQueues<1024, Message, Response> ProcessQueue;

#endif
