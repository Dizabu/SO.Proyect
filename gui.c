#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"
#include "vehicle.h"
#include "bridge.h"

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static TTF_Font* font_title = NULL;
static TTF_Font* font_normal = NULL;
static TTF_Font* font_small = NULL;


void gui_init()
{
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow(
        "Bridge Simulation - Traffic Control System",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1400,
        600,
        0
    );

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    font_title = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20);
    font_normal = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
    font_small = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11);

    if (!font_title || !font_normal || !font_small)
        printf("ERROR: Some fonts not loaded\n");
}

void draw_text_with_font(TTF_Font* font, const char* text, int x, int y, SDL_Color color)
{
    if (!font) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_text(const char* text, int x, int y)
{
    SDL_Color color = {255, 255, 255, 255};
    draw_text_with_font(font_normal, text, x, y, color);
}

void draw_text_colored(const char* text, int x, int y, SDL_Color color)
{
    draw_text_with_font(font_normal, text, x, y, color);
}

void draw_title(const char* text, int x, int y)
{
    SDL_Color color = {255, 215, 0, 255};
    draw_text_with_font(font_title, text, x, y, color);
}

void draw_statistics_panel(Bridge* bridge, Config* config)
{
    int panel_x = 20;
    int panel_y = 20;
    int line_height = 25;
    int y_offset = 0;

    // Panel background
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 200);
    SDL_Rect panel = {panel_x - 10, panel_y - 10, 280, 380};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    draw_title("=== STATISTICS ===", panel_x, panel_y);
    y_offset += line_height;

    // Mode
    char mode_text[100];
    SDL_Color mode_color;
    switch (bridge->mode) {
        case MODE_POLICE:
            sprintf(mode_text, "Mode: POLICE CONTROL");
            mode_color = (SDL_Color){255, 200, 0, 255};
            break;
        case MODE_SEMAFOROS:
            sprintf(mode_text, "Mode: TRAFFIC LIGHTS");
            mode_color = (SDL_Color){0, 200, 255, 255};
            break;
        default:
            sprintf(mode_text, "Mode: CARNAGE");
            mode_color = (SDL_Color){255, 100, 100, 255};
    }
    draw_text_colored(mode_text, panel_x, panel_y + y_offset, mode_color);
    y_offset += line_height;

    // Modo SEMAFOROS - información específica
    if (bridge->mode == MODE_SEMAFOROS) {
        int west_green = 0, east_green = 0;

        pthread_mutex_lock(&bridge->west_light_mutex);
        west_green = bridge->west_light_state;
        pthread_mutex_unlock(&bridge->west_light_mutex);

        pthread_mutex_lock(&bridge->east_light_mutex);
        east_green = bridge->east_light_state;
        pthread_mutex_unlock(&bridge->east_light_mutex);

        char west_light_text[100];
        if (west_green) {
            sprintf(west_light_text, "WEST Light: 🟢 GREEN (%d sec)", bridge->west_green_time);
            draw_text_colored(west_light_text, panel_x, panel_y + y_offset, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(west_light_text, "WEST Light: 🔴 RED");
            draw_text_colored(west_light_text, panel_x, panel_y + y_offset, (SDL_Color){255, 0, 0, 255});
        }
        y_offset += line_height;

        char east_light_text[100];
        if (east_green) {
            sprintf(east_light_text, "EAST Light: 🟢 GREEN (%d sec)", bridge->east_green_time);
            draw_text_colored(east_light_text, panel_x, panel_y + y_offset, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(east_light_text, "EAST Light: 🔴 RED");
            draw_text_colored(east_light_text, panel_x, panel_y + y_offset, (SDL_Color){255, 0, 0, 255});
        }
        y_offset += line_height;
    }

    // Modo POLICE - información específica
    if (bridge->mode == MODE_POLICE) {
        char police_east_text[100];
        if (bridge->police_east_active) {
            sprintf(police_east_text, "POLICE EAST: 🟢 ACTIVE (%d/%d)",
                    bridge->police_east_vehicles_passed, bridge->east_Ki);
            draw_text_colored(police_east_text, panel_x, panel_y + y_offset, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(police_east_text, "POLICE EAST: 🔴 INACTIVE");
            draw_text_colored(police_east_text, panel_x, panel_y + y_offset, (SDL_Color){255, 0, 0, 255});
        }
        y_offset += line_height;

        char police_west_text[100];
        if (bridge->police_west_active) {
            sprintf(police_west_text, "POLICE WEST: 🟢 ACTIVE (%d/%d)",
                    bridge->police_west_vehicles_passed, bridge->west_Ki);
            draw_text_colored(police_west_text, panel_x, panel_y + y_offset, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(police_west_text, "POLICE WEST: 🔴 INACTIVE");
            draw_text_colored(police_west_text, panel_x, panel_y + y_offset, (SDL_Color){255, 0, 0, 255});
        }
        y_offset += line_height;

        time_t now = time(NULL);
        int tiempo_east_sin_pasar = now - bridge->last_east_pass_time;
        int tiempo_west_sin_pasar = now - bridge->last_west_pass_time;

        char timeout_text[100];
        sprintf(timeout_text, "Timeout: EAST:%ds | WEST:%ds", tiempo_east_sin_pasar, tiempo_west_sin_pasar);
        draw_text(timeout_text, panel_x, panel_y + y_offset);
        y_offset += line_height;

        char amb_text[100];
        sprintf(amb_text, "Ambulances: EAST:%d | WEST:%d",
                bridge->waiting_ambulances_east, bridge->waiting_ambulances_west);
        draw_text(amb_text, panel_x, panel_y + y_offset);
        y_offset += line_height;
    }

    // Modo CARNAGE - información específica
    if (bridge->mode == MODE_CARNAGE) {
        char direction_text[100];
        if (bridge->current_direction == EAST) {
            sprintf(direction_text, "Direction: EAST → WEST");
        } else if (bridge->current_direction == WEST) {
            sprintf(direction_text, "Direction: WEST → EAST");
        } else {
            sprintf(direction_text, "Direction: NONE");
        }
        draw_text(direction_text, panel_x, panel_y + y_offset);
        y_offset += line_height;
    }

    // Información común a todos los modos
    char on_bridge[100];
    sprintf(on_bridge, "Vehicles on Bridge: %d", bridge->vehicles_on_bridge);
    draw_text(on_bridge, panel_x, panel_y + y_offset);
    y_offset += line_height;

    char east_wait[100];
    sprintf(east_wait, "EAST Waiting: %d", bridge->vehicles_east);
    SDL_Color east_color = {100, 200, 255, 255};
    draw_text_colored(east_wait, panel_x, panel_y + y_offset, east_color);
    y_offset += line_height;

    char west_wait[100];
    sprintf(west_wait, "WEST Waiting: %d", bridge->vehicles_west);
    SDL_Color west_color = {255, 150, 100, 255};
    draw_text_colored(west_wait, panel_x, panel_y + y_offset, west_color);
    y_offset += line_height;

    y_offset += 5;
    draw_text("--- Arrival Rates ---", panel_x, panel_y + y_offset);
    y_offset += line_height;

    char east_rate[100];
    double east_veh_per_min = 60.0 / config->east.arrival_mean;
    sprintf(east_rate, "EAST: %.1f veh/min (%.1f sec)", east_veh_per_min, config->east.arrival_mean);
    draw_text(east_rate, panel_x, panel_y + y_offset);
    y_offset += line_height;

    char west_rate[100];
    double west_veh_per_min = 60.0 / config->west.arrival_mean;
    sprintf(west_rate, "WEST: %.1f veh/min (%.1f sec)", west_veh_per_min, config->west.arrival_mean);
    draw_text(west_rate, panel_x, panel_y + y_offset);
    y_offset += line_height;

    y_offset += 5;
    draw_text("--- Vehicle Types ---", panel_x, panel_y + y_offset);
    y_offset += line_height;

    char east_amb[100];
    sprintf(east_amb, "EAST Ambulances: %d%%", config->east.ambulance_percentage);
    draw_text(east_amb, panel_x, panel_y + y_offset);
    y_offset += line_height;

    char west_amb[100];
    sprintf(west_amb, "WEST Ambulances: %d%%", config->west.ambulance_percentage);
    draw_text(west_amb, panel_x, panel_y + y_offset);
    y_offset += line_height;

    y_offset += 5;
    draw_text("--- Ki Values ---", panel_x, panel_y + y_offset);
    y_offset += line_height;

    char east_ki[100];
    sprintf(east_ki, "EAST Ki: %d", bridge->east_Ki);
    draw_text(east_ki, panel_x, panel_y + y_offset);
    y_offset += line_height;

    char west_ki[100];
    sprintf(west_ki, "WEST Ki: %d", bridge->west_Ki);
    draw_text(west_ki, panel_x, panel_y + y_offset);
}

void draw_waiting_queues(Bridge* bridge)
{
    int queue_start_x = 80;
    int east_queue_y = 330;
    int west_queue_y = 400;
    int vehicle_width = 20;
    int vehicle_height = 15;
    int spacing = 5;

    for (int i = 0; i < bridge->vehicles_east && i < 15; i++) {
        int x = queue_start_x + i * (vehicle_width + spacing);

        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_Rect bg = {x - 2, east_queue_y - 2, vehicle_width + 4, vehicle_height + 4};
        SDL_RenderFillRect(renderer, &bg);

        SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255);
        SDL_Rect vehicle = {x, east_queue_y, vehicle_width, vehicle_height};
        SDL_RenderFillRect(renderer, &vehicle);
    }

    for (int i = 0; i < bridge->vehicles_west && i < 15; i++) {
        int x = queue_start_x + i * (vehicle_width + spacing);

        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_Rect bg = {x - 2, west_queue_y - 2, vehicle_width + 4, vehicle_height + 4};
        SDL_RenderFillRect(renderer, &bg);

        SDL_SetRenderDrawColor(renderer, 255, 150, 100, 255);
        SDL_Rect vehicle = {x, west_queue_y, vehicle_width, vehicle_height};
        SDL_RenderFillRect(renderer, &vehicle);
    }

    draw_text_colored("EAST → WAITING QUEUE:", 80, 315, (SDL_Color){100, 200, 255, 255});
    draw_text_colored("WEST ← WAITING QUEUE:", 80, 385, (SDL_Color){255, 150, 100, 255});
}

void draw_police_ui(Bridge* bridge)
{
    int police_x = 1100;
    int police_y = 250;
    int size = 60;

    draw_title("POLICE CONTROL", police_x - 20, police_y - 60);

    // Policía EAST
    SDL_Rect east_police = {police_x, police_y, size, size};
    if (bridge->police_east_active) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        SDL_RenderFillRect(renderer, &east_police);
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = {police_x - i, police_y - i, size + i*2, size + i*2};
            SDL_RenderDrawRect(renderer, &glow);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderFillRect(renderer, &east_police);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &east_police);

    draw_text("👮", police_x + 22, police_y + 15);
    draw_text("EAST", police_x + 18, police_y + 40);

    char east_progress[50];
    sprintf(east_progress, "%d/%d", bridge->police_east_vehicles_passed, bridge->east_Ki);
    draw_text(east_progress, police_x + 22, police_y + 55);

    // Policía WEST
    SDL_Rect west_police = {police_x + size + 30, police_y, size, size};
    if (bridge->police_west_active) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        SDL_RenderFillRect(renderer, &west_police);
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = {police_x + size + 30 - i, police_y - i, size + i*2, size + i*2};
            SDL_RenderDrawRect(renderer, &glow);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderFillRect(renderer, &west_police);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &west_police);

    draw_text("👮", police_x + size + 30 + 22, police_y + 15);
    draw_text("WEST", police_x + size + 30 + 18, police_y + 40);

    char west_progress[50];
    sprintf(west_progress, "%d/%d", bridge->police_west_vehicles_passed, bridge->west_Ki);
    draw_text(west_progress, police_x + size + 30 + 22, police_y + 55);

    // Timeout bars
    time_t now = time(NULL);
    int tiempo_east_sin_pasar = now - bridge->last_east_pass_time;
    int tiempo_west_sin_pasar = now - bridge->last_west_pass_time;

    int bar_width = 200;
    int bar_x = police_x - 20;
    int bar_y = police_y + size + 20;

    char east_timeout_text[50];
    sprintf(east_timeout_text, "EAST timeout: %ds", tiempo_east_sin_pasar);
    draw_text(east_timeout_text, bar_x, bar_y - 15);

    int filled_east = (tiempo_east_sin_pasar * bar_width) / 10;
    if (filled_east > bar_width) filled_east = bar_width;
    SDL_Rect bar_bg_east = {bar_x, bar_y, bar_width, 15};
    SDL_Rect bar_fill_east = {bar_x, bar_y, filled_east, 15};

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &bar_bg_east);

    if (tiempo_east_sin_pasar >= 10) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
    }
    SDL_RenderFillRect(renderer, &bar_fill_east);

    int bar_y_west = bar_y + 25;
    char west_timeout_text[50];
    sprintf(west_timeout_text, "WEST timeout: %ds", tiempo_west_sin_pasar);
    draw_text(west_timeout_text, bar_x, bar_y_west - 15);

    int filled_west = (tiempo_west_sin_pasar * bar_width) / 10;
    if (filled_west > bar_width) filled_west = bar_width;
    SDL_Rect bar_bg_west = {bar_x, bar_y_west, bar_width, 15};
    SDL_Rect bar_fill_west = {bar_x, bar_y_west, filled_west, 15};

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &bar_bg_west);

    if (tiempo_west_sin_pasar >= 10) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
    }
    SDL_RenderFillRect(renderer, &bar_fill_west);
}

void draw_traffic_lights_ui(Bridge* bridge)
{
    int lights_x = 1100;
    int lights_y = 250;
    int size = 60;

    draw_title("TRAFFIC LIGHTS", lights_x - 20, lights_y - 60);

    // Semáforo EAST
    SDL_Rect east_light = {lights_x, lights_y, size, size};
    int east_green = 0;
    pthread_mutex_lock(&bridge->east_light_mutex);
    east_green = bridge->east_light_state;
    pthread_mutex_unlock(&bridge->east_light_mutex);

    if (east_green) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        SDL_RenderFillRect(renderer, &east_light);
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = {lights_x - i, lights_y - i, size + i*2, size + i*2};
            SDL_RenderDrawRect(renderer, &glow);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &east_light);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &east_light);

    draw_text("🚦", lights_x + 22, lights_y + 15);
    draw_text("EAST", lights_x + 18, lights_y + 40);
    char east_time[50];
    sprintf(east_time, "%ds", bridge->east_green_time);
    draw_text(east_time, lights_x + 22, lights_y + 55);

    // Semáforo WEST
    SDL_Rect west_light = {lights_x + size + 30, lights_y, size, size};
    int west_green = 0;
    pthread_mutex_lock(&bridge->west_light_mutex);
    west_green = bridge->west_light_state;
    pthread_mutex_unlock(&bridge->west_light_mutex);

    if (west_green) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        SDL_RenderFillRect(renderer, &west_light);
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = {lights_x + size + 30 - i, lights_y - i, size + i*2, size + i*2};
            SDL_RenderDrawRect(renderer, &glow);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &west_light);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &west_light);

    draw_text("🚦", lights_x + size + 30 + 22, lights_y + 15);
    draw_text("WEST", lights_x + size + 30 + 18, lights_y + 40);
    char west_time[50];
    sprintf(west_time, "%ds", bridge->west_green_time);
    draw_text(west_time, lights_x + size + 30 + 22, lights_y + 55);
}

void draw_carnage_ui(Bridge* bridge)
{
    int carnage_x = 1100;
    int carnage_y = 250;
    int size = 60;

    draw_title("CARNAGE MODE", carnage_x - 20, carnage_y - 60);

    // Mostrar dirección actual
    SDL_Rect direction_box = {carnage_x, carnage_y, size * 2 + 30, size};
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
    SDL_RenderFillRect(renderer, &direction_box);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &direction_box);

    char dir_text[50];
    if (bridge->current_direction == EAST) {
        sprintf(dir_text, "EAST → WEST");
        draw_text_colored(dir_text, carnage_x + 40, carnage_y + 25, (SDL_Color){0, 255, 0, 255});
    } else if (bridge->current_direction == WEST) {
        sprintf(dir_text, "WEST → EAST");
        draw_text_colored(dir_text, carnage_x + 40, carnage_y + 25, (SDL_Color){0, 255, 0, 255});
    } else {
        sprintf(dir_text, "WAITING");
        draw_text_colored(dir_text, carnage_x + 55, carnage_y + 25, (SDL_Color){255, 255, 0, 255});
    }

    draw_text("One direction at a time", carnage_x, carnage_y + size + 20);
}

void draw_bridge(Bridge* bridge)
{
    int segment_width = 15;
    int start_x = 350;
    int bridge_y = 200;
    int lane_height = 25;

    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_Rect bridge_bg = {start_x - 10, bridge_y - 10,
                          bridge->length * segment_width + 20, lane_height * 2 + 20};
    SDL_RenderFillRect(renderer, &bridge_bg);

    for (int i = 0; i < bridge->length; i++) {
        int x = start_x + i * segment_width;

        SDL_Rect west_lane = {x, bridge_y, segment_width - 1, lane_height};
        SDL_Rect east_lane = {x, bridge_y + lane_height, segment_width - 1, lane_height};

        // Colores según el modo
        if (bridge->mode == MODE_POLICE) {
            if (bridge->police_east_active) {
                SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else if (bridge->police_west_active) {
                SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 70, 70, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_RenderFillRect(renderer, &east_lane);
            }
        } else if (bridge->mode == MODE_SEMAFOROS) {
            int east_green = 0, west_green = 0;
            pthread_mutex_lock(&bridge->east_light_mutex);
            east_green = bridge->east_light_state;
            pthread_mutex_unlock(&bridge->east_light_mutex);
            pthread_mutex_lock(&bridge->west_light_mutex);
            west_green = bridge->west_light_state;
            pthread_mutex_unlock(&bridge->west_light_mutex);

            if (west_green) {
                SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else if (east_green) {
                SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 70, 70, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_RenderFillRect(renderer, &east_lane);
            }
        } else {
            if (bridge->current_direction == EAST) {
                SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else if (bridge->current_direction == WEST) {
                SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 70, 70, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_RenderFillRect(renderer, &east_lane);
            }
        }

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawLine(renderer, x, bridge_y + lane_height,
                          x + segment_width, bridge_y + lane_height);

        Vehicle* v = bridge->segment_vehicles[i];
        if (v != NULL) {
            int y = (v->dir == WEST) ? bridge_y : bridge_y + lane_height;

            if (v->type == VEHICLE_AMBULANCE)
                SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
            else if (v->dir == WEST)
                SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
            else
                SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);

            SDL_Rect car = {x + 1, y + 2, segment_width - 3, lane_height - 4};
            SDL_RenderFillRect(renderer, &car);

            if (v->type == VEHICLE_AMBULANCE) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawLine(renderer, x + segment_width/2, y + 5,
                                  x + segment_width/2, y + lane_height - 5);
                SDL_RenderDrawLine(renderer, x + 5, y + lane_height/2,
                                  x + segment_width - 5, y + lane_height/2);
            }
        }
    }

    draw_text_colored("<- WESTBOUND (to EAST)", start_x - 100, bridge_y + 5, (SDL_Color){255, 140, 0, 255});
    draw_text_colored("EASTBOUND (to WEST) ->", start_x - 100, bridge_y + lane_height + 5, (SDL_Color){0, 150, 255, 255});

    // Texto indicador de dirección permitida
    char dir_text[150];
    if (bridge->mode == MODE_POLICE) {
        if (bridge->police_east_active) {
            sprintf(dir_text, ">>>>> POLICE EAST: WEST → EAST ALLOWED (%d/%d) >>>>>",
                    bridge->police_east_vehicles_passed, bridge->east_Ki);
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else if (bridge->police_west_active) {
            sprintf(dir_text, "<<<<< POLICE WEST: EAST → WEST ALLOWED (%d/%d) <<<<<",
                    bridge->police_west_vehicles_passed, bridge->west_Ki);
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(dir_text, "⚠️ NO POLICE ACTIVE - Switching turn... ⚠️");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){255, 255, 0, 255});
        }
    } else if (bridge->mode == MODE_SEMAFOROS) {
        int east_green = 0, west_green = 0;
        pthread_mutex_lock(&bridge->east_light_mutex);
        east_green = bridge->east_light_state;
        pthread_mutex_unlock(&bridge->east_light_mutex);
        pthread_mutex_lock(&bridge->west_light_mutex);
        west_green = bridge->west_light_state;
        pthread_mutex_unlock(&bridge->west_light_mutex);

        if (west_green) {
            sprintf(dir_text, ">>>>> WEST LIGHT: GREEN for WEST → EAST >>>>>");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else if (east_green) {
            sprintf(dir_text, "<<<<< EAST LIGHT: GREEN for EAST → WEST <<<<<");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(dir_text, "⚠️ BOTH LIGHTS RED - Waiting... ⚠️");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){255, 255, 0, 255});
        }
    } else if (bridge->mode == MODE_CARNAGE) {
        if (bridge->current_direction == EAST) {
            sprintf(dir_text, ">>>>> CARNAGE MODE: WEST → EAST ALLOWED >>>>>");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else if (bridge->current_direction == WEST) {
            sprintf(dir_text, "<<<<< CARNAGE MODE: EAST → WEST ALLOWED <<<<<");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(dir_text, "⚠️ CARNAGE MODE: WAITING FOR DIRECTION ⚠️");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 150,
                            bridge_y - 30, (SDL_Color){255, 255, 0, 255});
        }
    }
}

void draw_legend()
{
    int legend_x = 1100;
    int legend_y = 20;
    int line_height = 20;
    int y_offset = 0;

    draw_title("LEGEND", legend_x, legend_y);
    y_offset += line_height + 5;

    SDL_Rect color_box = {legend_x, legend_y + y_offset, 15, 15};

    SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Eastbound Vehicle", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Westbound Vehicle", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Ambulance", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Allowed Direction", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    SDL_SetRenderDrawColor(renderer, 100, 80, 80, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Blocked Direction", legend_x + 20, legend_y + y_offset);
}

void gui_render(Bridge* bridge, Config* config)
{
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    draw_bridge(bridge);
    draw_statistics_panel(bridge, config);
    draw_waiting_queues(bridge);
    draw_legend();

    // Mostrar UI específica según el modo
    if (bridge->mode == MODE_SEMAFOROS) {
        draw_traffic_lights_ui(bridge);
    } else if (bridge->mode == MODE_POLICE) {
        draw_police_ui(bridge);
    } else if (bridge->mode == MODE_CARNAGE) {
        draw_carnage_ui(bridge);
    }

    draw_title("BRIDGE TRAFFIC SIMULATION", 500, 20);
    SDL_RenderPresent(renderer);
}

void* gui_run(Bridge* bridge, Config* config)
{
    gui_init();

    SDL_Event e;
    int running = 1;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        gui_render(bridge, config);
        SDL_Delay(33);
    }

    gui_destroy();
    return NULL;
}

void gui_destroy()
{
    if (font_title) TTF_CloseFont(font_title);
    if (font_normal) TTF_CloseFont(font_normal);
    if (font_small) TTF_CloseFont(font_small);
    TTF_Quit();

    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}