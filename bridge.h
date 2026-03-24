#ifndef BRIDGE_H
#define BRIDGE_H

#include <pthread.h>
#include <time.h>
#include "config.h"

struct Vehicle;

typedef struct Bridge {
    int length;
    int vehicles_on_bridge;
    int current_direction;

    int waiting_ambulances_east;
    int waiting_ambulances_west;
    int waiting_cars_east;
    int waiting_cars_west;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    pthread_mutex_t *segment_mutexes;
    struct Vehicle **segment_vehicles;

    SimulationMode mode;

    int light_direction;
    int east_green_time;
    int west_green_time;

    int vehicles_east;
    int vehicles_west;

    int cars_passed_in_turn;
    int current_turn_max;
    int east_Ki;
    int west_Ki;

    pthread_t police_east_thread;
    pthread_t police_west_thread;

    int police_east_active;
    int police_west_active;
    int police_east_vehicles_passed;
    int police_west_vehicles_passed;

    pthread_mutex_t east_light_mutex;
    pthread_mutex_t west_light_mutex;
    pthread_cond_t east_light_cond;
    pthread_cond_t west_light_cond;
    int east_light_state;
    int west_light_state;

    int vehicles_from_east_on_bridge;
    int vehicles_from_west_on_bridge;

    time_t last_east_pass_time;
    time_t last_west_pass_time;

} Bridge;

void bridge_init(Bridge* bridge, int length);
void bridge_enter(Bridge* bridge, struct Vehicle* v);
void bridge_leave(Bridge* bridge, struct Vehicle* v);

void* traffic_light_east_run(void* arg);
void* traffic_light_west_run(void* arg);
void* traffic_light_run(void* arg);

void* police_east_run(void* arg);
void* police_west_run(void* arg);

#endif