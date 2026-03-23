#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "bridge.h"
#include "traffic_generator.h"
#include "config.h"
#include "gui.h"


typedef struct {
    Bridge* bridge;
    Config* config;
} GuiThreadArgs;


void* gui_thread_wrapper(void* arg) {
    GuiThreadArgs* args = (GuiThreadArgs*)arg;
    return gui_run(args->bridge, args->config);
}

int main() {

    printf("DEBUG: Starting simulation\n");

    srand(time(NULL));
    printf("DEBUG: Random seed initialized\n");



    Config config;

    if (load_config("config.txt", &config) != 0) {
        printf("ERROR: Could not load configuration file\n");
        return 1;
    }

    printf("DEBUG: Configuration loaded\n");



    Bridge bridge;

    bridge_init(&bridge, config.bridge_length);

    printf("DEBUG: Bridge initialized with length = %d meters\n", bridge.length);

    bridge.mode = config.mode;

    bridge.cars_passed_in_turn = 0;
    bridge.east_Ki = config.east.Ki;
    bridge.west_Ki = config.west.Ki;

    bridge.east_green_time = config.east.green_time;
    bridge.west_green_time = config.west.green_time;

    bridge.light_direction = EAST;

    printf("DEBUG: Simulation mode = %d\n", bridge.mode);
    bridge.cars_passed_in_turn = 0;
    bridge.current_turn_max = config.east.Ki;
    printf("DEBUG: East green = %d | West green = %d\n",
           bridge.east_green_time, bridge.west_green_time);



    pthread_t light_thread;

    if (bridge.mode == MODE_SEMAFOROS) {
        // Crear hilos independientes para cada semáforo
        pthread_t east_light_thread;
        pthread_t west_light_thread;

        pthread_create(&east_light_thread, NULL, traffic_light_east_run, &bridge);
        pthread_create(&west_light_thread, NULL, traffic_light_west_run, &bridge);

        printf("DEBUG: Independent traffic lights started (EAST and WEST)\n");
        printf("   EAST green time: %d seconds\n", bridge.east_green_time);
        printf("   WEST green time: %d seconds\n", bridge.west_green_time);
    }



    pthread_t gui_thread;


    GuiThreadArgs gui_args;
    gui_args.bridge = &bridge;
    gui_args.config = &config;

    if (pthread_create(&gui_thread, NULL, gui_thread_wrapper, &gui_args) != 0) {
        printf("ERROR: Could not start GUI thread\n");
        return 1;
    }

    printf("DEBUG: GUI started with configuration\n");



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


    pthread_join(east_thread, NULL);
    pthread_join(west_thread, NULL);

    printf("DEBUG: Generators finished\n");

    pthread_join(gui_thread, NULL);

    printf("DEBUG: Simulation finished\n");

    return 0;
}