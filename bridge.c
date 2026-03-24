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

    pthread_mutex_init(&bridge->east_light_mutex, NULL);
    pthread_mutex_init(&bridge->west_light_mutex, NULL);
    pthread_cond_init(&bridge->east_light_cond, NULL);
    pthread_cond_init(&bridge->west_light_cond, NULL);

    bridge->east_light_state = 0;
    bridge->west_light_state = 0;
    bridge->vehicles_from_east_on_bridge = 0;
    bridge->vehicles_from_west_on_bridge = 0;

    bridge->police_east_active = 0;
    bridge->police_west_active = 0;
    bridge->police_east_vehicles_passed = 0;
    bridge->police_west_vehicles_passed = 0;

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

        if (bridge->mode == MODE_POLICE) {
            int hay_vehiculos_sentido_contrario_en_puente = 0;
            if (v->dir == EAST) {
                hay_vehiculos_sentido_contrario_en_puente = (bridge->vehicles_from_east_on_bridge > 0);
            } else {
                hay_vehiculos_sentido_contrario_en_puente = (bridge->vehicles_from_west_on_bridge > 0);
            }

            int police_active = (v->dir == EAST) ? bridge->police_east_active : bridge->police_west_active;
            int hay_ambulancias_contrarias = (v->dir == EAST) ? (bridge->waiting_ambulances_east > 0) : (bridge->waiting_ambulances_west > 0);

            time_t now = time(NULL);
            int tiempo_sin_pasar_otro_lado = (v->dir == EAST) ? (now - bridge->last_east_pass_time) : (now - bridge->last_west_pass_time);
            int timeout_activado = (tiempo_sin_pasar_otro_lado >= 10);

            if (v->type == VEHICLE_AMBULANCE) {
                int opposite_vehicles_on_bridge = (v->dir == EAST) ? bridge->vehicles_from_east_on_bridge : bridge->vehicles_from_west_on_bridge;

                if (opposite_vehicles_on_bridge == 0 || timeout_activado) {
                    if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                        if (v->dir == EAST) {
                            bridge->last_west_pass_time = now;
                            bridge->police_east_vehicles_passed++;
                        } else {
                            bridge->last_east_pass_time = now;
                            bridge->police_west_vehicles_passed++;
                        }
                        break;
                    }
                }
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (hay_ambulancias_contrarias && !timeout_activado) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (!police_active) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (hay_vehiculos_sentido_contrario_en_puente) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            int vehicles_passed = (v->dir == EAST) ? bridge->police_east_vehicles_passed : bridge->police_west_vehicles_passed;
            int max_vehicles = (v->dir == EAST) ? bridge->east_Ki : bridge->west_Ki;

            if (vehicles_passed >= max_vehicles) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                if (v->dir == EAST) {
                    bridge->last_west_pass_time = now;
                    bridge->police_east_vehicles_passed++;
                } else {
                    bridge->last_east_pass_time = now;
                    bridge->police_west_vehicles_passed++;
                }
                break;
            }

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        if (bridge->mode == MODE_CARNAGE) {
            if (bridge->vehicles_on_bridge > 0 && bridge->current_direction != v->dir) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0)
                break;

            pthread_cond_wait(&bridge->cond, &bridge->mutex);
            continue;
        }

        if (bridge->mode == MODE_SEMAFOROS) {
            int light_is_green = 0;
            if (v->dir == EAST) {
                pthread_mutex_lock(&bridge->west_light_mutex);
                light_is_green = (bridge->west_light_state == 1);
                pthread_mutex_unlock(&bridge->west_light_mutex);
            } else {
                pthread_mutex_lock(&bridge->east_light_mutex);
                light_is_green = (bridge->east_light_state == 1);
                pthread_mutex_unlock(&bridge->east_light_mutex);
            }

            int opposite_vehicles_on_bridge = (v->dir == EAST) ? bridge->vehicles_from_east_on_bridge : bridge->vehicles_from_west_on_bridge;

            if (v->type == VEHICLE_AMBULANCE) {
                if (opposite_vehicles_on_bridge == 0) {
                    if (pthread_mutex_trylock(&bridge->segment_mutexes[first_segment]) == 0) {
                        break;
                    }
                }
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (!light_is_green) {
                pthread_cond_wait(&bridge->cond, &bridge->mutex);
                continue;
            }

            if (opposite_vehicles_on_bridge > 0) {
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

    pthread_mutex_unlock(&bridge->mutex);
}

void bridge_leave(Bridge* bridge, Vehicle* v) {
    pthread_mutex_lock(&bridge->mutex);

    bridge->segment_vehicles[v->current_segment] = NULL;
    pthread_mutex_unlock(&bridge->segment_mutexes[v->current_segment]);

    bridge->vehicles_on_bridge--;

    if (v->dir == EAST) {
        bridge->vehicles_from_west_on_bridge--;
        bridge->vehicles_east--;
    } else {
        bridge->vehicles_from_east_on_bridge--;
        bridge->vehicles_west--;
    }

    if (bridge->vehicles_on_bridge == 0) {
        bridge->current_direction = -1;
        pthread_cond_broadcast(&bridge->cond);
    }

    pthread_cond_broadcast(&bridge->cond);
    pthread_mutex_unlock(&bridge->mutex);
}

void* traffic_light_east_run(void* arg) {
    Bridge* bridge = (Bridge*) arg;

    while (1) {
        pthread_mutex_lock(&bridge->west_light_mutex);
        while (bridge->west_light_state == 1) {
            pthread_mutex_unlock(&bridge->west_light_mutex);
            usleep(100000);
            pthread_mutex_lock(&bridge->west_light_mutex);
        }
        pthread_mutex_unlock(&bridge->west_light_mutex);

        pthread_mutex_lock(&bridge->east_light_mutex);
        bridge->east_light_state = 1;
        pthread_cond_broadcast(&bridge->east_light_cond);
        pthread_mutex_unlock(&bridge->east_light_mutex);

        sleep(bridge->east_green_time);

        pthread_mutex_lock(&bridge->east_light_mutex);
        bridge->east_light_state = 0;
        pthread_mutex_unlock(&bridge->east_light_mutex);

        pthread_mutex_lock(&bridge->mutex);
        while (bridge->vehicles_from_east_on_bridge > 0) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }
        pthread_mutex_unlock(&bridge->mutex);

        sleep(1);
    }
    return NULL;
}

void* traffic_light_west_run(void* arg) {
    Bridge* bridge = (Bridge*) arg;

    while (1) {
        pthread_mutex_lock(&bridge->east_light_mutex);
        while (bridge->east_light_state == 1) {
            pthread_mutex_unlock(&bridge->east_light_mutex);
            usleep(100000);
            pthread_mutex_lock(&bridge->east_light_mutex);
        }
        pthread_mutex_unlock(&bridge->east_light_mutex);

        pthread_mutex_lock(&bridge->west_light_mutex);
        bridge->west_light_state = 1;
        pthread_cond_broadcast(&bridge->west_light_cond);
        pthread_mutex_unlock(&bridge->west_light_mutex);

        sleep(bridge->west_green_time);

        pthread_mutex_lock(&bridge->west_light_mutex);
        bridge->west_light_state = 0;
        pthread_mutex_unlock(&bridge->west_light_mutex);

        pthread_mutex_lock(&bridge->mutex);
        while (bridge->vehicles_from_west_on_bridge > 0) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }
        pthread_mutex_unlock(&bridge->mutex);

        sleep(1);
    }
    return NULL;
}
void* traffic_light_run(void* arg) {
    return NULL;
}

void* police_east_run(void* arg) {
    Bridge* bridge = (Bridge*) arg;

    while (1) {
        pthread_mutex_lock(&bridge->mutex);

        while (bridge->vehicles_on_bridge > 0 || bridge->police_west_active) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }

        bridge->police_east_active = 1;
        bridge->police_west_active = 0;
        bridge->police_east_vehicles_passed = 0;

        pthread_mutex_unlock(&bridge->mutex);

        int vehicles_to_pass = bridge->east_Ki;
        int last_passed = 0;

        while (vehicles_to_pass > 0) {
            pthread_mutex_lock(&bridge->mutex);
            int hay_vehiculos = (bridge->vehicles_west > 0);

            if (bridge->police_east_vehicles_passed > last_passed) {
                vehicles_to_pass--;
                last_passed = bridge->police_east_vehicles_passed;
            }
            pthread_mutex_unlock(&bridge->mutex);

            if (!hay_vehiculos && vehicles_to_pass > 0) {
                usleep(100000);
                continue;
            }

            pthread_mutex_lock(&bridge->mutex);
            pthread_cond_broadcast(&bridge->cond);
            pthread_mutex_unlock(&bridge->mutex);

            usleep(50000);
        }

        pthread_mutex_lock(&bridge->mutex);
        bridge->police_east_active = 0;
        pthread_cond_broadcast(&bridge->cond);
        pthread_mutex_unlock(&bridge->mutex);

        sleep(1);
    }

    return NULL;
}

void* police_west_run(void* arg) {
    Bridge* bridge = (Bridge*) arg;

    while (1) {
        pthread_mutex_lock(&bridge->mutex);

        while (bridge->vehicles_on_bridge > 0 || bridge->police_east_active) {
            pthread_cond_wait(&bridge->cond, &bridge->mutex);
        }

        bridge->police_east_active = 0;
        bridge->police_west_active = 1;
        bridge->police_west_vehicles_passed = 0;

        pthread_mutex_unlock(&bridge->mutex);

        int vehicles_to_pass = bridge->west_Ki;
        int last_passed = 0;

        while (vehicles_to_pass > 0) {
            pthread_mutex_lock(&bridge->mutex);
            int hay_vehiculos = (bridge->vehicles_east > 0);

            if (bridge->police_west_vehicles_passed > last_passed) {
                vehicles_to_pass--;
                last_passed = bridge->police_west_vehicles_passed;
            }
            pthread_mutex_unlock(&bridge->mutex);

            if (!hay_vehiculos && vehicles_to_pass > 0) {
                usleep(100000);
                continue;
            }

            pthread_mutex_lock(&bridge->mutex);
            pthread_cond_broadcast(&bridge->cond);
            pthread_mutex_unlock(&bridge->mutex);

            usleep(50000);
        }

        pthread_mutex_lock(&bridge->mutex);
        bridge->police_west_active = 0;
        pthread_cond_broadcast(&bridge->cond);
        pthread_mutex_unlock(&bridge->mutex);

        sleep(1);
    }

    return NULL;
}