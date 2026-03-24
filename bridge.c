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

    time_t now = time(NULL);
    bridge->last_east_pass_time = now;
    bridge->last_west_pass_time = now;


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

        // ========== MODO POLICE CON TIMEOUT (10 SEGUNDOS) ==========
        // ========== MODO POLICE CON TIMEOUT (10 SEGUNDOS) ==========
    if (bridge->mode == MODE_POLICE) {

        int hay_contrarios = (v->dir == EAST)
            ? (bridge->vehicles_west > 0)
            : (bridge->vehicles_east > 0);

        // Verificar si hay ambulancias esperando en la dirección contraria
        int hay_ambulancias_contrarias = (v->dir == EAST)
            ? (bridge->waiting_ambulances_west > 0)
            : (bridge->waiting_ambulances_east > 0);

        // Obtener tiempo actual
        time_t now = time(NULL);

        // Calcular tiempo desde que el OTRO LADO pasó por última vez
        // Si v->dir == EAST, significa que el vehículo quiere ir al ESTE (viene del WEST)
        // Entonces el "otro lado" es el EAST (vehículos que vienen del EAST y van al WEST)
        int tiempo_sin_pasar_otro_lado;
        if (v->dir == EAST) {
            // Vehículo quiere ir al ESTE, el otro lado es EAST
            tiempo_sin_pasar_otro_lado = (now - bridge->last_east_pass_time);
        } else {
            // Vehículo quiere ir al OESTE, el otro lado es WEST
            tiempo_sin_pasar_otro_lado = (now - bridge->last_west_pass_time);
        }

        // TIMEOUT FIJO DE 10 SEGUNDOS
        int timeout_activado = (tiempo_sin_pasar_otro_lado >= 10);

        // Debug: imprimir cada 5 segundos aproximadamente
        static time_t last_debug = 0;
        if (now - last_debug >= 5) {
            last_debug = now;
            printf("🔍 DEBUG: last_east=%ld, last_west=%ld, now=%ld, diff_east=%ld, diff_west=%ld\n",
                   bridge->last_east_pass_time, bridge->last_west_pass_time, now,
                   now - bridge->last_east_pass_time, now - bridge->last_west_pass_time);
        }

        if (timeout_activado && hay_ambulancias_contrarias) {
            printf("⏰ TIMEOUT! %d seconds without %s vehicles, allowing vehicles\n",
                   tiempo_sin_pasar_otro_lado,
                   (v->dir == EAST) ? "EAST" : "WEST");
        }

        // REGLA PARA AMBULANCIAS
        if (v->type == VEHICLE_AMBULANCE) {
            // Ambulancias pueden pasar si no hay vehículos en sentido contrario
            // o si el timeout está activado
            if ((bridge->vehicles_on_bridge == 0 ||
                 bridge->current_direction == v->dir) &&
                (!hay_contrarios || timeout_activado)) {
                if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                    // Actualizar tiempo de último paso para ESTE lado
                    if (v->dir == EAST) {
                        bridge->last_west_pass_time = now;
                        printf("🚨 AMBULANCE %d passing from WEST (updated last_west_pass_time to %ld)\n",
                               v->id, now);
                    } else {
                        bridge->last_east_pass_time = now;
                        printf("🚨 AMBULANCE %d passing from EAST (updated last_east_pass_time to %ld)\n",
                               v->id, now);
                    }
                    break;
                }
            }
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // REGLA PARA VEHÍCULOS NORMALES
        // Si hay ambulancias esperando en sentido contrario, verificar timeout
        if (hay_ambulancias_contrarias && !timeout_activado) {
            printf("🚗 Vehicle %d waiting: ambulances in opposite direction (timeout in %d sec)\n",
                   v->id, 10 - tiempo_sin_pasar_otro_lado);
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // Verificar dirección del semáforo policía
        if (v->dir != bridge->light_direction) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // Verificar límite de vehículos por turno
        if (bridge->cars_passed_in_turn >= bridge->current_turn_max) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // Verificar vehículos en sentido contrario
        if (hay_contrarios) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        // Intentar entrar
        if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
            bridge->cars_passed_in_turn++;
            // Actualizar tiempo de último paso para ESTE lado
            if (v->dir == EAST) {
                bridge->last_west_pass_time = now;
                printf("🚗 Vehicle %d passing from WEST (updated last_west_pass_time to %ld)\n", v->id, now);
            } else {
                bridge->last_east_pass_time = now;
                printf("🚗 Vehicle %d passing from EAST (updated last_east_pass_time to %ld)\n", v->id, now);
            }
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

        // ========== MODO SEMAFOROS (sin cambios) ==========
        if (bridge->mode == MODE_SEMAFOROS) {

            int light_is_green = 0;
            int hay_ambulancias_esperando = 0;

            if (v->dir == EAST) {
                pthread_mutex_lock(&bridge->west_light_mutex);
                light_is_green = (bridge->west_light_state == 1);
                pthread_mutex_unlock(&bridge->west_light_mutex);
                hay_ambulancias_esperando = (bridge->waiting_ambulances_west > 0);
            } else {
                pthread_mutex_lock(&bridge->east_light_mutex);
                light_is_green = (bridge->east_light_state == 1);
                pthread_mutex_unlock(&bridge->east_light_mutex);
                hay_ambulancias_esperando = (bridge->waiting_ambulances_east > 0);
            }

            if (v->type == VEHICLE_AMBULANCE && hay_ambulancias_esperando) {
                int opposite_vehicles = 0;
                if (v->dir == EAST) {
                    opposite_vehicles = bridge->vehicles_from_east_on_bridge;
                } else {
                    opposite_vehicles = bridge->vehicles_from_west_on_bridge;
                }

                if (opposite_vehicles > 0) {
                    pthread_cond_wait(&bridge->cond, &bridge->mutex);
                    continue;
                }

                if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                    break;
                }

                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (!light_is_green) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            int opposite_vehicles = 0;
            if (v->dir == EAST) {
                opposite_vehicles = bridge->vehicles_from_east_on_bridge;
            } else {
                opposite_vehicles = bridge->vehicles_from_west_on_bridge;
            }

            if (opposite_vehicles > 0) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
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

                // NUEVO: Cuando cambia el turno, reiniciar los tiempos
                time_t now = time(NULL);
                bridge->last_east_pass_time = now;
                bridge->last_west_pass_time = now;
                printf("⏰ TIMEOUT counters reset after turn switch (now: %ld)\n", now);

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
