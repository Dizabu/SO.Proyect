#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "bridge.h"
#include "traffic_generator.h"
#include "config.h"
#include "gui.h"

int main() {

    printf("DEBUG: Starting simulation\n");

    srand(time(NULL));
    printf("DEBUG: Random seed initialized\n");


    // -------- Cargar configuración --------
    Config config;

    if (load_config("config.txt", &config) != 0) {
        printf("ERROR: Could not load configuration file\n");
        return 1;
    }

    printf("DEBUG: Configuration loaded\n");


    // -------- Inicializar puente --------
    Bridge bridge;

    bridge_init(&bridge, config.bridge_length);

    printf("DEBUG: Bridge initialized with length = %d meters\n", bridge.length);

    bridge.mode = config.mode;

    bridge.east_green_time = config.east.green_time;
    bridge.west_green_time = config.west.green_time;

    bridge.light_direction = EAST;

    printf("DEBUG: Simulation mode = %d\n", bridge.mode);
    printf("DEBUG: East green = %d | West green = %d\n",
           bridge.east_green_time, bridge.west_green_time);


    // -------- INICIAR SEMAFORO --------
    pthread_t light_thread;

    if (bridge.mode == MODE_SEMAFOROS)
    {
        pthread_create(&light_thread, NULL, traffic_light_run, &bridge);
        printf("DEBUG: Traffic light thread started\n");
    }


    // -------- INICIAR GUI --------
    pthread_t gui_thread;

    if (pthread_create(&gui_thread, NULL, gui_run, &bridge) != 0) {
        printf("ERROR: Could not start GUI thread\n");
        return 1;
    }

    printf("DEBUG: GUI started\n");


    // -------- Generadores de tráfico --------
    GeneratorArgs east_args;
    east_args.dir = EAST;
    east_args.bridge = &bridge;
    east_args.config = &config.east;

    GeneratorArgs west_args;
    west_args.dir = WEST;
    west_args.bridge = &bridge;
    west_args.config = &config.west;


    pthread_t east_thread;
    pthread_t west_thread;

    pthread_create(&east_thread, NULL, traffic_generator, &east_args);
    pthread_create(&west_thread, NULL, traffic_generator, &west_args);

    printf("DEBUG: Both generators are now running\n");


    // Esperar generadores
    pthread_join(east_thread, NULL);
    pthread_join(west_thread, NULL);


    // Esperar que el usuario cierre la GUI
    pthread_join(gui_thread, NULL);


    printf("DEBUG: Simulation finished\n");

    return 0;
}