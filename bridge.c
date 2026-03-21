    #include "bridge.h"
    #include "vehicle.h"
    #include <stdio.h>
    #include <stdlib.h>
#include <unistd.h>

void bridge_init(Bridge* bridge, int length)
{
    bridge->length = length;
    bridge->vehicles_on_bridge = 0;
    bridge->current_direction = -1;

    // -------- ambulancias --------
    bridge->waiting_ambulances_east = 0;
    bridge->waiting_ambulances_west = 0;

    // -------- carros esperando (IMPORTANTE PARA POLICE) --------
    bridge->waiting_cars_east = 0;
    bridge->waiting_cars_west = 0;

    // -------- carros en el puente por dirección --------
    bridge->vehicles_east = 0;
    bridge->vehicles_west = 0;

    pthread_mutex_init(&bridge->mutex, NULL);
    pthread_cond_init(&bridge->cond, NULL);

    // -------- segmentos del puente --------
    bridge->segment_mutexes = malloc(sizeof(pthread_mutex_t) * length);
    bridge->segment_vehicles = malloc(sizeof(struct Vehicle*) * length);

    for (int i = 0; i < length; i++) {
        pthread_mutex_init(&bridge->segment_mutexes[i], NULL);
        bridge->segment_vehicles[i] = NULL;
    }

    // -------- modo por defecto --------
    bridge->mode = MODE_CARNAGE;

    // -------- SEMÁFOROS --------
    bridge->light_direction = EAST;
    bridge->east_green_time = 0;
    bridge->west_green_time = 0;

    // -------- POLICE MODE --------

    // cuántos han pasado en el turno actual
    bridge->cars_passed_in_turn = 0;

    // Ki (se sobreescriben en main desde config)
    bridge->east_Ki = 1;
    bridge->west_Ki = 1;

    // límite actual del turno (empieza EAST)
    bridge->current_turn_max = bridge->east_Ki;
}

void bridge_enter(Bridge* bridge, Vehicle* v) {
    pthread_mutex_lock(&bridge->mutex);

    if (v->type == VEHICLE_AMBULANCE) {
        if (v->dir == EAST)
            bridge->waiting_ambulances_east++;
        else
            bridge->waiting_ambulances_west++;
    }

    int first_segment = (v->dir == EAST) ? 0 : bridge->length - 1;

    while (1) {

        // puente lleno
        if (bridge->vehicles_on_bridge == bridge->length) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // -------- MODO SEMAFOROS --------
        if (bridge->mode == MODE_SEMAFOROS) {

            int hay_contrarios = (v->dir == EAST)
                ? (bridge->vehicles_west > 0)
                : (bridge->vehicles_east > 0);

            if (hay_contrarios) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (bridge->light_direction != v->dir) {

                if (!(v->type == VEHICLE_AMBULANCE &&
                      bridge->vehicles_on_bridge == 0)) {
                    pthread_cond_wait(&bridge->cond, &bridge->mutex);
                    continue;
                }
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0)
                break;

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // -------- MODO CARNAGE --------
        if (bridge->mode == MODE_CARNAGE) {

            if (bridge->vehicles_on_bridge > 0 &&
                bridge->current_direction != v->dir) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0)
                break;

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // -------- MODO POLICE --------
        if (bridge->mode == MODE_POLICE) {

            // 🚑 ambulancias
            if (v->type == VEHICLE_AMBULANCE) {

                if (bridge->vehicles_on_bridge == 0 ||
                    bridge->current_direction == v->dir) {

                    if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0)
                        break;
                }

                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // 🚗 carros normales

            // no es su turno
            if (v->dir != bridge->light_direction) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // ya se cumplió Ki
            if (bridge->cars_passed_in_turn >= bridge->current_turn_max) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // intentar entrar
            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                bridge->cars_passed_in_turn++; // ✅ SOLO AQUÍ
                break;
            }

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }
    }

    // -------- ACTUALIZAR ESTADO --------

    if (bridge->vehicles_on_bridge == 0) {
        bridge->current_direction = v->dir;
    }

    bridge->vehicles_on_bridge++;

    if (v->dir == EAST)
        bridge->vehicles_east++;
    else
        bridge->vehicles_west++;

    if (v->type == VEHICLE_AMBULANCE) {
        if (v->dir == EAST)
            bridge->waiting_ambulances_east--;
        else
            bridge->waiting_ambulances_west--;
    }

    v->current_segment = first_segment;
    bridge->segment_vehicles[first_segment] = v;

    printf("Vehicle %d ENTERED (%s %s) | vehicles on bridge: %d/%d (E:%d W:%d)\n",
           v->id,
           v->dir == EAST ? "EAST" : "WEST",
           v->type == VEHICLE_AMBULANCE ? "AMBULANCE" : "CAR",
           bridge->vehicles_on_bridge,
           bridge->length,
           bridge->vehicles_east,
           bridge->vehicles_west);

    pthread_mutex_unlock(&bridge->mutex);
}

void bridge_leave(Bridge* bridge, Vehicle* v) {
    pthread_mutex_lock(&bridge->mutex);

    bridge->segment_vehicles[v->current_segment] = NULL;
    pthread_mutex_unlock(&bridge->segment_mutexes[v->current_segment]);

    bridge->vehicles_on_bridge--;

    // -------- MODO POLICE --------
    if (bridge->mode == MODE_POLICE) {
        // SOLO cambiar si el puente está vacío
        if (bridge->vehicles_on_bridge == 0)
        {
            // SOLO cambiar cuando se cumpla Ki
            if (bridge->cars_passed_in_turn >= bridge->current_turn_max)
            {
                // cambiar turno
                bridge->light_direction =
                    (bridge->light_direction == EAST) ? WEST : EAST;

                bridge->cars_passed_in_turn = 0;

                // actualizar Ki del nuevo turno
                if (bridge->light_direction == EAST)
                    bridge->current_turn_max = bridge->east_Ki;
                else
                    bridge->current_turn_max = bridge->west_Ki;

                printf("POLICE: Switching to %s\n",
                       bridge->light_direction == EAST ? "EAST" : "WEST");

                pthread_cond_broadcast(&bridge->cond); // 🔥 IMPORTANTE
            }
        }
    }

    if (v->dir == EAST)
        bridge->vehicles_east--;
    else
        bridge->vehicles_west--;

    printf("Vehicle %d LEFT (%s %s) | vehicles on bridge = %d/%d (E:%d W:%d)\n",
           v->id, v->dir == EAST ? "EAST" : "WEST",
           v->type == VEHICLE_AMBULANCE ? "AMBULANCE" : "CAR",
           bridge->vehicles_on_bridge, bridge->length,
           bridge->vehicles_east, bridge->vehicles_west);

    if (bridge->vehicles_on_bridge == 0) {
        bridge->current_direction = -1;
        printf("Bridge is now EMPTY\n");
    }

    pthread_cond_broadcast(&bridge->cond);
    pthread_mutex_unlock(&bridge->mutex);
}

void* traffic_light_run(void* arg)
{
    Bridge* bridge = (Bridge*) arg;

    while (1)
    {
        int green_time;
        int current_dir;

        pthread_mutex_lock(&bridge->mutex);
        current_dir = bridge->light_direction;
        if (current_dir == EAST)
            green_time = bridge->east_green_time;
        else
            green_time = bridge->west_green_time;
        pthread_mutex_unlock(&bridge->mutex);

        sleep(green_time);

        pthread_mutex_lock(&bridge->mutex);

        bridge->light_direction = -1;
        printf("Traffic light turned RED, waiting for bridge to empty...\n");
        pthread_cond_broadcast(&bridge->cond);

        while (bridge->vehicles_on_bridge > 0) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }

        bridge->light_direction = (current_dir == EAST) ? WEST : EAST;
        printf("Traffic light switched to %s\n",
               bridge->light_direction == EAST ? "EAST" : "WEST");
        pthread_cond_broadcast(&bridge->cond);

        pthread_mutex_unlock(&bridge->mutex);
    }

    return NULL;
}