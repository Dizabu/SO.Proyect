#ifndef VEHICLE_H
#define VEHICLE_H

typedef struct Bridge Bridge;

typedef enum {
    EAST,
    WEST
} Direction;

typedef enum {
    VEHICLE_CAR,
    VEHICLE_AMBULANCE
} VehicleType;

typedef struct Vehicle {
    int id;
    Direction dir;
    VehicleType type;

    double speed_kmh;

    int current_segment;

    Bridge* bridge;

} Vehicle;

void start_vehicle_thread(Vehicle* v);

#endif