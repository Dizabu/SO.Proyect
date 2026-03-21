#ifndef BRIDGE_H
#define BRIDGE_H

#include <pthread.h>
#include "config.h"

struct Vehicle;

typedef struct Bridge {
    int length;
    int vehicles_on_bridge;
    int current_direction;

    int waiting_ambulances_east;
    int waiting_ambulances_west;

    // 👉 NECESARIO PARA POLICE
    int waiting_cars_east;
    int waiting_cars_west;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    pthread_mutex_t *segment_mutexes;
    struct Vehicle **segment_vehicles;

    SimulationMode mode;

    // -------- SEMAFOROS --------
    int light_direction;
    int east_green_time;
    int west_green_time;

    // -------- VEHICULOS EN PUENTE --------
    int vehicles_east;
    int vehicles_west;

    // -------- POLICE MODE --------
    int cars_passed_in_turn;
    int current_turn_max;   // Ki actual
    int east_Ki;
    int west_Ki;

} Bridge;

void bridge_init(Bridge* bridge, int length);
void bridge_enter(Bridge* bridge, struct Vehicle* v);
void bridge_leave(Bridge* bridge, struct Vehicle* v);

/* 👇 AGREGA ESTO */
void* traffic_light_run(void* arg);

#endif