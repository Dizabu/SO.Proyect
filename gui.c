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
    SDL_Rect panel = {panel_x - 10, panel_y - 10, 280, 340};
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

    // Para modo SEMAFOROS, mostrar estado de los semáforos independientes
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

    // Para modo POLICE, mostrar información del turno
    if (bridge->mode == MODE_POLICE) {
        char turn_text[100];
        sprintf(turn_text, "Current Turn: %s",
                bridge->light_direction == EAST ? "EAST →" : "WEST ←");
        draw_text(turn_text, panel_x, panel_y + y_offset);
        y_offset += line_height;

        char progress_text[100];
        sprintf(progress_text, "Progress: %d / %d",
                bridge->cars_passed_in_turn, bridge->current_turn_max);
        draw_text(progress_text, panel_x, panel_y + y_offset);
        y_offset += line_height;
    }

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

    // ========== CORREGIDO: Usar arrival_mean en lugar de vehicles_per_minute ==========
    y_offset += 5;
    draw_text("--- Arrival Rates ---", panel_x, panel_y + y_offset);
    y_offset += line_height;

    char east_rate[100];
    // Calcular vehículos por minuto a partir de arrival_mean (segundos)
    double east_veh_per_min = 60.0 / config->east.arrival_mean;
    sprintf(east_rate, "EAST: %.1f veh/min (%.1f sec)", east_veh_per_min, config->east.arrival_mean);
    draw_text(east_rate, panel_x, panel_y + y_offset);
    y_offset += line_height;

    char west_rate[100];
    double west_veh_per_min = 60.0 / config->west.arrival_mean;
    sprintf(west_rate, "WEST: %.1f veh/min (%.1f sec)", west_veh_per_min, config->west.arrival_mean);
    draw_text(west_rate, panel_x, panel_y + y_offset);
    y_offset += line_height;

    // Mostrar porcentaje de ambulancias
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
    int police_y = 300;
    int size = 40;

    draw_title("POLICE CONTROL", police_x - 40, police_y - 50);

    SDL_Rect west_police = {police_x, police_y, size, size};
    if (bridge->light_direction == WEST)
        SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &west_police);
    draw_text("WEST", police_x + 10, police_y + 15);

    SDL_Rect east_police = {police_x + size + 20, police_y, size, size};
    if (bridge->light_direction == EAST)
        SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &east_police);
    draw_text("EAST", police_x + size + 30, police_y + 15);

    int bar_width = 200;
    int filled = 0;
    if (bridge->current_turn_max > 0)
        filled = (bridge->cars_passed_in_turn * bar_width) / bridge->current_turn_max;

    SDL_Rect bar_bg = {police_x - 20, police_y + size + 20, bar_width, 20};
    SDL_Rect bar_fill = {police_x - 20, police_y + size + 20, filled, 20};

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &bar_bg);

    SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
    SDL_RenderFillRect(renderer, &bar_fill);

    char progress_text[50];
    sprintf(progress_text, "Progress: %d%%", (filled * 100) / bar_width);
    draw_text(progress_text, police_x - 20, police_y + size + 45);
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

        if (bridge->mode == MODE_POLICE) {
            if (bridge->light_direction == EAST) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 100, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
                SDL_RenderFillRect(renderer, &west_lane);
                SDL_SetRenderDrawColor(renderer, 60, 60, 100, 255);
                SDL_RenderFillRect(renderer, &east_lane);
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
            SDL_RenderFillRect(renderer, &west_lane);
            SDL_RenderFillRect(renderer, &east_lane);
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


    draw_text_colored("<- WESTBOUND", start_x - 100, bridge_y + 5, (SDL_Color){255, 140, 0, 255});
    draw_text_colored("EASTBOUND ->", start_x - 100, bridge_y + lane_height + 5, (SDL_Color){0, 150, 255, 255});


    if (bridge->mode == MODE_POLICE) {
        char dir_text[50];
        if (bridge->light_direction == EAST) {
            sprintf(dir_text, ">>>>> EASTBOUND ALLOWED >>>>>");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 100,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
        } else {
            sprintf(dir_text, "<<<<< WESTBOUND ALLOWED <<<<<");
            draw_text_colored(dir_text, start_x + bridge->length * segment_width / 2 - 100,
                            bridge_y - 30, (SDL_Color){0, 255, 0, 255});
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

    SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Active Police", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Inactive Police", legend_x + 20, legend_y + y_offset);
}


void draw_traffic_lights(Bridge* bridge)
{
    int start_x = 350;
    int segment_width = 15;
    int west_light_x = start_x - 40;
    int east_light_x = start_x + bridge->length * segment_width + 20;
    int y = 210;

    // Semáforo WEST (controla vehículos que van hacia EAST)
    SDL_Rect west_light = {west_light_x, y, 20, 20};
    // Semáforo EAST (controla vehículos que van hacia WEST)
    SDL_Rect east_light = {east_light_x, y, 20, 20};

    // Verificar el estado real de cada semáforo independiente
    int west_light_green = 0;
    int east_light_green = 0;

    pthread_mutex_lock(&bridge->west_light_mutex);
    west_light_green = (bridge->west_light_state == 1);
    pthread_mutex_unlock(&bridge->west_light_mutex);

    pthread_mutex_lock(&bridge->east_light_mutex);
    east_light_green = (bridge->east_light_state == 1);
    pthread_mutex_unlock(&bridge->east_light_mutex);

    // Dibujar semáforo WEST
    if (west_light_green) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Verde
        SDL_RenderFillRect(renderer, &west_light);
        // Añadir brillo alrededor cuando está verde
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = {west_light_x - i, y - i, 20 + i*2, 20 + i*2};
            SDL_RenderDrawRect(renderer, &glow);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Rojo
        SDL_RenderFillRect(renderer, &west_light);
    }

    // Dibujar semáforo EAST
    if (east_light_green) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Verde
        SDL_RenderFillRect(renderer, &east_light);
        // Añadir brillo alrededor cuando está verde
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = {east_light_x - i, y - i, 20 + i*2, 20 + i*2};
            SDL_RenderDrawRect(renderer, &glow);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Rojo
        SDL_RenderFillRect(renderer, &east_light);
    }

    // Dibujar bordes blancos
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &west_light);
    SDL_RenderDrawRect(renderer, &east_light);

    // Etiquetas
    draw_text("WEST LIGHT", west_light_x - 10, y - 25);
    draw_text("(to EAST)", west_light_x - 5, y - 15);
    draw_text("EAST LIGHT", east_light_x + 5, y - 25);
    draw_text("(to WEST)", east_light_x + 10, y - 15);

    // Mostrar tiempos de verde si están en verde
    if (west_light_green) {
        char time_text[50];
        sprintf(time_text, "%d sec", bridge->west_green_time);
        draw_text(time_text, west_light_x + 2, y + 25);
    }
    if (east_light_green) {
        char time_text[50];
        sprintf(time_text, "%d sec", bridge->east_green_time);
        draw_text(time_text, east_light_x + 2, y + 25);
    }
}



void gui_render(Bridge* bridge, Config* config)
{
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    draw_bridge(bridge);
    draw_statistics_panel(bridge, config);
    draw_waiting_queues(bridge);
    draw_legend();

    // Mostrar diferentes elementos según el modo
    if (bridge->mode == MODE_SEMAFOROS) {
        draw_traffic_lights(bridge);  // Ahora muestra semáforos independientes
    } else if (bridge->mode == MODE_POLICE) {
        draw_police_ui(bridge);
    } else if (bridge->mode == MODE_CARNAGE) {
        int carnage_x = 1100;
        int carnage_y = 200;
        draw_title("CARNAGE MODE", carnage_x, carnage_y);
        draw_text("One direction at a time", carnage_x, carnage_y + 30);
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