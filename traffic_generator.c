#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "traffic_generator.h"

static int vehicle_id = 0;

double exponential(double mean) {
    double u = (double)rand() / (RAND_MAX + 1.0);
    return -mean * log(u);
}

void* traffic_generator(void* arg) {
    GeneratorArgs* args = (GeneratorArgs*) arg;
    Direction dir = args->dir;
    Bridge* bridge = args->bridge;
    DirectionConfig* cfg = args->config;

    int samples = 0;
    double total_wait = 0;

    while (1) {
        Vehicle* v = (Vehicle*) malloc(sizeof(Vehicle));
        v->id = vehicle_id++;
        v->dir = dir;
        v->bridge = bridge;
        v->current_segment = -1;

        int r = rand() % 100;
        if (r < cfg->ambulance_percentage)
            v->type = VEHICLE_AMBULANCE;
        else
            v->type = VEHICLE_CAR;

        double speed = cfg->avg_speed + (rand() % (2 * cfg->speed_variation + 1) - cfg->speed_variation);
        if (speed < cfg->min_speed) speed = cfg->min_speed;
        if (speed > cfg->max_speed) speed = cfg->max_speed;
        v->speed_kmh = speed;

        printf("Vehicle %d arriving from %s (%s, speed %.2f km/h)\n",
               v->id,
               dir == EAST ? "EAST" : "WEST",
               v->type == VEHICLE_AMBULANCE ? "AMBULANCE" : "CAR",
               v->speed_kmh);

        start_vehicle_thread(v);

        double wait = exponential(cfg->arrival_mean);
        samples++;
        total_wait += wait;

        if (samples % 20 == 0) {
            double observed_mean = total_wait / samples;
            printf("\n[Generator %s stats]\n", dir == EAST ? "EAST" : "WEST");
            printf("samples: %d\n", samples);
            printf("expected mean: %.3f sec\n", cfg->arrival_mean);
            printf("observed mean: %.3f sec\n\n", observed_mean);
        }

        usleep(wait * 1000000);
    }
    return NULL;
}