#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include "bridge.h"
#include "vehicle.h"
#include "config.h"

typedef struct {
    Direction dir;
    Bridge* bridge;
    DirectionConfig* config;
} GeneratorArgs;

void* traffic_generator(void* arg);

#endif