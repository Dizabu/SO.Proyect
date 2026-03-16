#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int load_config(const char* filename, Config* cfg) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open config file %s\n", filename);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        char* trimmed = line;
        while (isspace(*trimmed)) trimmed++;
        if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n')
            continue;

        // Find '=' separator
        char* equal_sign = strchr(trimmed, '=');
        if (!equal_sign) continue;

        // Split key and value
        *equal_sign = '\0';
        char* key = trimmed;
        char* value = equal_sign + 1;

        // Trim trailing spaces from key
        char* end = key + strlen(key) - 1;
        while (end > key && isspace(*end)) {
            *end = '\0';
            end--;
        }

        // Trim trailing spaces and newline from value
        end = value + strlen(value) - 1;
        while (end > value && (isspace(*end) || *end == '\n')) {
            *end = '\0';
            end--;
        }

        if (strcmp(key, "bridge_length") == 0) {
            cfg->bridge_length = atoi(value);
        }
        else if (strcmp(key, "simulation_mode") == 0) {
            if (strcmp(value, "semaforos") == 0)
                cfg->mode = MODE_SEMAFOROS;
            else if (strcmp(value, "police") == 0)
                cfg->mode = MODE_POLICE;
            else if (strcmp(value, "carnage") == 0)
                cfg->mode = MODE_CARNAGE;
            else {
                fprintf(stderr, "Unknown simulation mode: %s\n", value);
                return -1;
            }
        }

        else if (strcmp(key, "east_arrival_mean") == 0)
            cfg->east.arrival_mean = atof(value);
        else if (strcmp(key, "east_avg_speed") == 0)
            cfg->east.avg_speed = atof(value);
        else if (strcmp(key, "east_Ki") == 0)
            cfg->east.Ki = atoi(value);
        else if (strcmp(key, "east_green_time") == 0)
            cfg->east.green_time = atoi(value);
        else if (strcmp(key, "east_min_speed") == 0)
            cfg->east.min_speed = atoi(value);
        else if (strcmp(key, "east_max_speed") == 0)
            cfg->east.max_speed = atoi(value);
        else if (strcmp(key, "east_ambulance_percentage") == 0)
            cfg->east.ambulance_percentage = atoi(value);
        else if (strcmp(key, "east_speed_variation") == 0)
            cfg->east.speed_variation = atoi(value);

        else if (strcmp(key, "west_arrival_mean") == 0)
            cfg->west.arrival_mean = atof(value);
        else if (strcmp(key, "west_avg_speed") == 0)
            cfg->west.avg_speed = atof(value);
        else if (strcmp(key, "west_Ki") == 0)
            cfg->west.Ki = atoi(value);
        else if (strcmp(key, "west_green_time") == 0)
            cfg->west.green_time = atoi(value);
        else if (strcmp(key, "west_min_speed") == 0)
            cfg->west.min_speed = atoi(value);
        else if (strcmp(key, "west_max_speed") == 0)
            cfg->west.max_speed = atoi(value);
        else if (strcmp(key, "west_ambulance_percentage") == 0)
            cfg->west.ambulance_percentage = atoi(value);
        else if (strcmp(key, "west_speed_variation") == 0)
            cfg->west.speed_variation = atoi(value);

        // Ignore unknown keys silently
    }

    fclose(f);
    return 0;
}