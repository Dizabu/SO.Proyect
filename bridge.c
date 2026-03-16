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

    bridge->waiting_ambulances_east = 0;
    bridge->waiting_ambulances_west = 0;

    pthread_mutex_init(&bridge->mutex, NULL);
    pthread_cond_init(&bridge->cond, NULL);

    // -------- segmentos del puente --------
    bridge->segment_mutexes = malloc(sizeof(pthread_mutex_t) * length);
    bridge->segment_vehicles = malloc(sizeof(struct Vehicle*) * length);

    for (int i = 0; i < length; i++) {
        pthread_mutex_init(&bridge->segment_mutexes[i], NULL);
        bridge->segment_vehicles[i] = NULL;
    }

    // -------- valores por defecto del modo --------
    bridge->mode = MODE_CARNAGE;

    // -------- semáforos --------
    bridge->light_direction = EAST;

    bridge->east_green_time = 0;
    bridge->west_green_time = 0;
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
            if (bridge->vehicles_on_bridge == bridge->length)
            {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // -------- MODO SEMAFORO --------
            if (bridge->mode == MODE_SEMAFOROS &&
                bridge->light_direction != v->dir)
            {
                printf("Vehicle %d WAITING (red light)\n", v->id);
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // dirección contraria (modo carnage)
            if (bridge->mode == MODE_CARNAGE &&
                bridge->vehicles_on_bridge > 0 &&
                bridge->current_direction != v->dir)
            {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0)
                break;

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }

        // Actualizar estado global
        if (bridge->vehicles_on_bridge == 0) {
            bridge->current_direction = v->dir;
        }
        bridge->vehicles_on_bridge++;


        if (v->type == VEHICLE_AMBULANCE) {
            if (v->dir == EAST)
                bridge->waiting_ambulances_east--;
            else
                bridge->waiting_ambulances_west--;
        }

        // Guardar el segmento actual en el vehículo (el primero)
        v->current_segment = first_segment;
        bridge->segment_vehicles[first_segment] = v;

        printf("Vehicle %d ENTERED (%s %s) | vehicles on bridge: %d/%d\n",
               v->id, v->dir == EAST ? "EAST" : "WEST",
               v->type == VEHICLE_AMBULANCE ? "AMBULANCE" : "CAR",
               bridge->vehicles_on_bridge, bridge->length);

        pthread_mutex_unlock(&bridge->mutex);
    }

    void bridge_leave(Bridge* bridge, Vehicle* v) {
        pthread_mutex_lock(&bridge->mutex);

        // quitar vehículo del mapa visual
        bridge->segment_vehicles[v->current_segment] = NULL;

        // liberar el segmento final
        pthread_mutex_unlock(&bridge->segment_mutexes[v->current_segment]);

        bridge->vehicles_on_bridge--;

        printf("Vehicle %d LEFT (%s %s) | vehicles on bridge = %d/%d\n",
               v->id, v->dir == EAST ? "EAST" : "WEST",
               v->type == VEHICLE_AMBULANCE ? "AMBULANCE" : "CAR",
               bridge->vehicles_on_bridge, bridge->length);

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
        int green;

        pthread_mutex_lock(&bridge->mutex);

        if (bridge->light_direction == EAST)
            green = bridge->east_green_time;
        else
            green = bridge->west_green_time;

        pthread_mutex_unlock(&bridge->mutex);

        // esperar tiempo del semáforo
        sleep(green);
        //commit

        pthread_mutex_lock(&bridge->mutex);

        // cambiar dirección
        bridge->light_direction =
            (bridge->light_direction == EAST) ? WEST : EAST;

        printf("Traffic light switched to %s\n",
               bridge->light_direction == EAST ? "EAST" : "WEST");

        pthread_cond_broadcast(&bridge->cond);

        pthread_mutex_unlock(&bridge->mutex);
    }

    return NULL;
}