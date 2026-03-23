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

    bridge->waiting_cars_east = 0;
    bridge->waiting_cars_west = 0;

    bridge->vehicles_east = 0;
    bridge->vehicles_west = 0;

    pthread_mutex_init(&bridge->mutex, NULL);
    pthread_cond_init(&bridge->cond, NULL);

    bridge->segment_mutexes = malloc(sizeof(pthread_mutex_t) * length);
    bridge->segment_vehicles = malloc(sizeof(struct Vehicle*) * length);

    for (int i = 0; i < length; i++) {
        pthread_mutex_init(&bridge->segment_mutexes[i], NULL);
        bridge->segment_vehicles[i] = NULL;
    }

    bridge->mode = MODE_CARNAGE;

    bridge->light_direction = EAST;
    bridge->east_green_time = 0;
    bridge->west_green_time = 0;

    bridge->cars_passed_in_turn = 0;

    bridge->east_Ki = 1;
    bridge->west_Ki = 1;

    bridge->current_turn_max = bridge->east_Ki;

    // NUEVO: Inicializar semáforos independientes
    pthread_mutex_init(&bridge->east_light_mutex, NULL);
    pthread_mutex_init(&bridge->west_light_mutex, NULL);
    pthread_cond_init(&bridge->east_light_cond, NULL);
    pthread_cond_init(&bridge->west_light_cond, NULL);

    bridge->east_light_state = 0;  // ROJO inicial
    bridge->west_light_state = 0;  // ROJO inicial
    bridge->vehicles_from_east_on_bridge = 0;
    bridge->vehicles_from_west_on_bridge = 0;
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

        if (bridge->vehicles_on_bridge == bridge->length) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // ========== MODO SEMAFOROS (SIMPLIFICADO) ==========
        if (bridge->mode == MODE_SEMAFOROS) {

            // Verificar el estado del semáforo correspondiente
            int light_is_green = 0;

            if (v->dir == EAST) {
                // Vehículo va hacia EAST (viene de WEST) - espera semáforo WEST
                pthread_mutex_lock(&bridge->west_light_mutex);
                light_is_green = (bridge->west_light_state == 1);
                pthread_mutex_unlock(&bridge->west_light_mutex);
            } else {
                // Vehículo va hacia WEST (viene de EAST) - espera semáforo EAST
                pthread_mutex_lock(&bridge->east_light_mutex);
                light_is_green = (bridge->east_light_state == 1);
                pthread_mutex_unlock(&bridge->east_light_mutex);
            }

            // REGLA 1: El semáforo debe estar en VERDE
            if (!light_is_green) {
                printf("🚦 Vehicle %d waiting at RED light\n", v->id);
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // REGLA 2: El puente debe estar completamente VACÍO
            if (bridge->vehicles_on_bridge > 0) {
                printf("🚦 Vehicle %d waiting: bridge has %d vehicles\n", v->id, bridge->vehicles_on_bridge);
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            // REGLA 3: Intentar bloquear el primer segmento
            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                break;
            }

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // ========== MODO CARNAGE (sin cambios) ==========
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

        // ========== MODO POLICE (sin cambios) ==========
        if (bridge->mode == MODE_POLICE) {

            int hay_contrarios = (v->dir == EAST)
                ? (bridge->vehicles_west > 0)
                : (bridge->vehicles_east > 0);

            if (v->type == VEHICLE_AMBULANCE) {

                if ((bridge->vehicles_on_bridge == 0 ||
                     bridge->current_direction == v->dir) && !hay_contrarios) {
                    if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0)
                        break;
                     }
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (v->dir != bridge->light_direction) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (bridge->cars_passed_in_turn >= bridge->current_turn_max) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (hay_contrarios) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                bridge->cars_passed_in_turn++;
                break;
            }

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }
    }

    if (bridge->vehicles_on_bridge == 0) {
        bridge->current_direction = v->dir;
    }

    bridge->vehicles_on_bridge++;

    // Actualizar contadores de dirección para los semáforos independientes
    if (v->dir == EAST) {
        bridge->vehicles_from_west_on_bridge++;
        bridge->vehicles_east++;
    } else {
        bridge->vehicles_from_east_on_bridge++;
        bridge->vehicles_west++;
    }

    if (v->type == VEHICLE_AMBULANCE) {
        if (v->dir == EAST)
            bridge->waiting_ambulances_east--;
        else
            bridge->waiting_ambulances_west--;
    }

    v->current_segment = first_segment;
    bridge->segment_vehicles[first_segment] = v;

    printf("🚗 Vehicle %d ENTERED (%s %s) | on bridge: %d/%d (E:%d W:%d)\n",
           v->id,
           v->dir == EAST ? "EAST→" : "WEST←",
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

    // Actualizar contadores de dirección
    if (v->dir == EAST) {
        bridge->vehicles_from_west_on_bridge--;
        bridge->vehicles_east--;
    } else {
        bridge->vehicles_from_east_on_bridge--;
        bridge->vehicles_west--;
    }

    if (bridge->mode == MODE_POLICE) {
        if (bridge->vehicles_on_bridge == 0) {
            if (bridge->cars_passed_in_turn >= bridge->current_turn_max) {
                bridge->light_direction =
                    (bridge->light_direction == EAST) ? WEST : EAST;
                bridge->cars_passed_in_turn = 0;
                if (bridge->light_direction == EAST)
                    bridge->current_turn_max = bridge->east_Ki;
                else
                    bridge->current_turn_max = bridge->west_Ki;
                printf("POLICE: Switching to %s\n",
                       bridge->light_direction == EAST ? "EAST" : "WEST");
                pthread_cond_broadcast(&bridge->cond);
            }
        }
    }

    printf("Vehicle %d LEFT (%s %s) | on bridge: %d/%d (E:%d W:%d)\n",
           v->id, v->dir == EAST ? "EAST" : "WEST",
           v->type == VEHICLE_AMBULANCE ? "AMBULANCE" : "CAR",
           bridge->vehicles_on_bridge, bridge->length,
           bridge->vehicles_east, bridge->vehicles_west);

    if (bridge->vehicles_on_bridge == 0) {
        bridge->current_direction = -1;
        printf("Bridge is now EMPTY\n");
        // Notificar a los semáforos que el puente está vacío
        pthread_cond_broadcast(&bridge->cond);
    }

    pthread_cond_broadcast(&bridge->cond);
    pthread_mutex_unlock(&bridge->mutex);
}


// Modificar traffic_light_east_run
void* traffic_light_east_run(void* arg) {
    Bridge* bridge = (Bridge*) arg;

    while (1) {
        // Esperar a que el semáforo OESTE esté en rojo
        pthread_mutex_lock(&bridge->west_light_mutex);
        while (bridge->west_light_state == 1) {
            pthread_mutex_unlock(&bridge->west_light_mutex);
            usleep(100000);  // Esperar 0.1 segundos
            pthread_mutex_lock(&bridge->west_light_mutex);
        }
        pthread_mutex_unlock(&bridge->west_light_mutex);

        // Poner semáforo ESTE en VERDE
        pthread_mutex_lock(&bridge->east_light_mutex);
        bridge->east_light_state = 1;
        printf("\n🚦 [EAST] 🟢 GREEN for %d seconds\n", bridge->east_green_time);
        pthread_cond_broadcast(&bridge->east_light_cond);
        pthread_mutex_unlock(&bridge->east_light_mutex);

        sleep(bridge->east_green_time);

        // Cambiar a ROJO
        pthread_mutex_lock(&bridge->east_light_mutex);
        bridge->east_light_state = 0;
        printf("🚦 [EAST] 🔴 RED\n");
        pthread_mutex_unlock(&bridge->east_light_mutex);

        // Esperar a que los vehículos del ESTE terminen
        pthread_mutex_lock(&bridge->mutex);
        while (bridge->vehicles_from_east_on_bridge > 0) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }
        pthread_mutex_unlock(&bridge->mutex);

        sleep(1);
    }
    return NULL;
}

// Similar para WEST
void* traffic_light_west_run(void* arg) {
    Bridge* bridge = (Bridge*) arg;

    while (1) {
        // Esperar a que el semáforo ESTE esté en rojo
        pthread_mutex_lock(&bridge->east_light_mutex);
        while (bridge->east_light_state == 1) {
            pthread_mutex_unlock(&bridge->east_light_mutex);
            usleep(100000);
            pthread_mutex_lock(&bridge->east_light_mutex);
        }
        pthread_mutex_unlock(&bridge->east_light_mutex);

        // Poner semáforo OESTE en VERDE
        pthread_mutex_lock(&bridge->west_light_mutex);
        bridge->west_light_state = 1;
        printf("\n🚦 [WEST] 🟢 GREEN for %d seconds\n", bridge->west_green_time);
        pthread_cond_broadcast(&bridge->west_light_cond);
        pthread_mutex_unlock(&bridge->west_light_mutex);

        sleep(bridge->west_green_time);

        // Cambiar a ROJO
        pthread_mutex_lock(&bridge->west_light_mutex);
        bridge->west_light_state = 0;
        printf("🚦 [WEST] 🔴 RED\n");
        pthread_mutex_unlock(&bridge->west_light_mutex);

        // Esperar a que los vehículos del OESTE terminen
        pthread_mutex_lock(&bridge->mutex);
        while (bridge->vehicles_from_west_on_bridge > 0) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }
        pthread_mutex_unlock(&bridge->mutex);

        sleep(1);
    }
    return NULL;
}

// Mantener la función original por compatibilidad (pero no se usa con el nuevo modo)
void* traffic_light_run(void* arg) {
    // Esta función ya no se usa con la nueva implementación
    // Se mantiene por si algún código antiguo la llama
    return NULL;
}
