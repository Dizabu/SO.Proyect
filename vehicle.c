#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "vehicle.h"
#include "bridge.h"   // ← ESTA LÍNEA FALTA

static void* vehicle_run(void* arg) {
    Vehicle* v = arg;

    bridge_enter(v->bridge, v);

    // tiempo por metro
    double time_per_meter = 3.6 / v->speed_kmh;

    int last_segment = (v->dir == EAST) ? v->bridge->length - 1 : 0;
    int step = (v->dir == EAST) ? 1 : -1;

    while (v->current_segment != last_segment) {

        int old_segment = v->current_segment;
        int next_segment = old_segment + step;

        // bloquear el siguiente segmento
        pthread_mutex_lock(&v->bridge->segment_mutexes[next_segment]);

        // simular tiempo de movimiento
        usleep(time_per_meter * 1000000);

        // liberar el segmento anterior
        pthread_mutex_unlock(&v->bridge->segment_mutexes[old_segment]);

        // actualizar posición
        v->current_segment = next_segment;

        // actualizar mapa de vehículos del puente (para la GUI)
        pthread_mutex_lock(&v->bridge->mutex);

        v->bridge->segment_vehicles[next_segment] = v;
        v->bridge->segment_vehicles[old_segment] = NULL;

        pthread_mutex_unlock(&v->bridge->mutex);

        printf("Vehicle %d moved to segment %d\n", v->id, v->current_segment);
    }

    // último metro antes de salir
    usleep(time_per_meter * 1000000);

    bridge_leave(v->bridge, v);

    free(v);
    return NULL;
}

void start_vehicle_thread(Vehicle* v) {
    pthread_t thread;
    pthread_create(&thread, NULL, vehicle_run, v);
    pthread_detach(thread);
}