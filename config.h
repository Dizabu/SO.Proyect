#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    MODE_SEMAFOROS,
    MODE_POLICE,
    MODE_CARNAGE
} SimulationMode;

typedef struct {
    double arrival_mean;
    int avg_speed;
    int Ki;

    int green_time;

    int min_speed;
    int max_speed;

    int ambulance_percentage;
    int speed_variation;

} DirectionConfig;

typedef struct {
    int bridge_length;

    DirectionConfig east;
    DirectionConfig west;

    SimulationMode mode;
} Config;

int load_config(const char* filename, Config* cfg);

#endif