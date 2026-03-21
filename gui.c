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

/* -------- INIT -------- */

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

    // Intentar cargar diferentes fuentes
    font_title = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20);
    font_normal = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
    font_small = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11);

    if (!font_title || !font_normal || !font_small)
        printf("ERROR: Some fonts not loaded\n");
}

/* -------- TEXT DRAWING FUNCTIONS -------- */

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
    SDL_Color color = {255, 215, 0, 255}; // Gold color
    draw_text_with_font(font_title, text, x, y, color);
}

/* -------- DRAW STATISTICS PANEL -------- */

void draw_statistics_panel(Bridge* bridge, Config* config)
{
    int panel_x = 20;
    int panel_y = 20;
    int line_height = 25;
    int y_offset = 0;

    // Panel background
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 200);
    SDL_Rect panel = {panel_x - 10, panel_y - 10, 280, 280};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    draw_title("=== STATISTICS ===", panel_x, panel_y);
    y_offset += line_height;

    // Mode
    char mode_text[100];
    SDL_Color mode_color;
    if (bridge->mode == MODE_POLICE) {
        sprintf(mode_text, "Mode: POLICE CONTROL");
        mode_color = (SDL_Color){255, 200, 0, 255};
    } else {
        sprintf(mode_text, "Mode: TRAFFIC LIGHTS");
        mode_color = (SDL_Color){0, 200, 255, 255};
    }
    draw_text_colored(mode_text, panel_x, panel_y + y_offset, mode_color);
    y_offset += line_height;

    // Current turn
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

    // Vehicles on bridge
    char on_bridge[100];
    sprintf(on_bridge, "Vehicles on Bridge: %d", bridge->vehicles_on_bridge);
    draw_text(on_bridge, panel_x, panel_y + y_offset);
    y_offset += line_height;

    // Waiting queues
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

    // Generation rates (usando valores simulados si no existen)
    y_offset += 5;
    draw_text("--- Generation Rates ---", panel_x, panel_y + y_offset);
    y_offset += line_height;

    // Comentado temporalmente - mostrar valores por defecto
    char east_rate[100];
    sprintf(east_rate, "EAST: %.1f veh/min", 10.0);  // Valor por defecto
    draw_text(east_rate, panel_x, panel_y + y_offset);
    y_offset += line_height;

    char west_rate[100];
    sprintf(west_rate, "WEST: %.1f veh/min", 10.0);  // Valor por defecto
    draw_text(west_rate, panel_x, panel_y + y_offset);
    y_offset += line_height;

    // Ki values
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

/* -------- DRAW WAITING QUEUES VISUALLY -------- */

void draw_waiting_queues(Bridge* bridge)
{
    int queue_start_x = 80;
    int east_queue_y = 330;
    int west_queue_y = 400;
    int vehicle_width = 20;
    int vehicle_height = 15;
    int spacing = 5;

    // Draw EAST queue (right side waiting to go WEST)
    for (int i = 0; i < bridge->vehicles_east && i < 15; i++) {
        int x = queue_start_x + i * (vehicle_width + spacing);

        // Queue background
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_Rect bg = {x - 2, east_queue_y - 2, vehicle_width + 4, vehicle_height + 4};
        SDL_RenderFillRect(renderer, &bg);

        // Vehicle
        SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255);
        SDL_Rect vehicle = {x, east_queue_y, vehicle_width, vehicle_height};
        SDL_RenderFillRect(renderer, &vehicle);
    }

    // Draw WEST queue (left side waiting to go EAST)
    for (int i = 0; i < bridge->vehicles_west && i < 15; i++) {
        int x = queue_start_x + i * (vehicle_width + spacing);

        // Queue background
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_Rect bg = {x - 2, west_queue_y - 2, vehicle_width + 4, vehicle_height + 4};
        SDL_RenderFillRect(renderer, &bg);

        // Vehicle
        SDL_SetRenderDrawColor(renderer, 255, 150, 100, 255);
        SDL_Rect vehicle = {x, west_queue_y, vehicle_width, vehicle_height};
        SDL_RenderFillRect(renderer, &vehicle);
    }

    // Draw queue labels
    draw_text_colored("EAST → WAITING QUEUE:", 80, 315, (SDL_Color){100, 200, 255, 255});
    draw_text_colored("WEST ← WAITING QUEUE:", 80, 385, (SDL_Color){255, 150, 100, 255});
}

/* -------- DRAW POLICE CONTROL UI -------- */

void draw_police_ui(Bridge* bridge)
{
    int police_x = 1100;
    int police_y = 300;
    int size = 40;

    // Title
    draw_title("POLICE CONTROL", police_x - 40, police_y - 50);

    // West police box
    SDL_Rect west_police = {police_x, police_y, size, size};
    if (bridge->light_direction == WEST)
        SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &west_police);
    draw_text("WEST", police_x + 10, police_y + 15);

    // East police box
    SDL_Rect east_police = {police_x + size + 20, police_y, size, size};
    if (bridge->light_direction == EAST)
        SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &east_police);
    draw_text("EAST", police_x + size + 30, police_y + 15);

    // Progress bar
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

/* -------- DRAW BRIDGE -------- */

void draw_bridge(Bridge* bridge)
{
    int segment_width = 15;  // Más grande
    int start_x = 350;
    int bridge_y = 200;
    int lane_height = 25;

    // Bridge background
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_Rect bridge_bg = {start_x - 10, bridge_y - 10,
                          bridge->length * segment_width + 20, lane_height * 2 + 20};
    SDL_RenderFillRect(renderer, &bridge_bg);

    // Draw each segment
    for (int i = 0; i < bridge->length; i++) {
        int x = start_x + i * segment_width;

        // Westbound lane (top)
        SDL_Rect west_lane = {x, bridge_y, segment_width - 1, lane_height};
        // Eastbound lane (bottom)
        SDL_Rect east_lane = {x, bridge_y + lane_height, segment_width - 1, lane_height};

        // Color based on mode and direction
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

        // Draw lane separator
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawLine(renderer, x, bridge_y + lane_height,
                          x + segment_width, bridge_y + lane_height);

        // Draw vehicle if present
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

            // Mark ambulance with cross
            if (v->type == VEHICLE_AMBULANCE) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawLine(renderer, x + segment_width/2, y + 5,
                                  x + segment_width/2, y + lane_height - 5);
                SDL_RenderDrawLine(renderer, x + 5, y + lane_height/2,
                                  x + segment_width - 5, y + lane_height/2);
            }
        }
    }

    // Bridge labels
    draw_text_colored("← WESTBOUND", start_x - 100, bridge_y + 5, (SDL_Color){255, 140, 0, 255});
    draw_text_colored("EASTBOUND →", start_x - 100, bridge_y + lane_height + 5, (SDL_Color){0, 150, 255, 255});

    // Direction indicators
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

/* -------- DRAW LEGEND -------- */

void draw_legend()
{
    int legend_x = 1100;
    int legend_y = 20;
    int line_height = 20;
    int y_offset = 0;

    draw_title("LEGEND", legend_x, legend_y);
    y_offset += line_height + 5;

    // Colors
    SDL_Rect color_box = {legend_x, legend_y + y_offset, 15, 15};

    // Eastbound
    SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Eastbound Vehicle", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    // Westbound
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Westbound Vehicle", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    // Ambulance
    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Ambulance", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    // Police active
    SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Active Police", legend_x + 20, legend_y + y_offset);
    y_offset += line_height;

    // Police inactive
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &color_box);
    draw_text("Inactive Police", legend_x + 20, legend_y + y_offset);
}

/* -------- MAIN RENDER FUNCTION -------- */

void gui_render(Bridge* bridge, Config* config)
{
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    // Draw all components
    draw_bridge(bridge);
    draw_statistics_panel(bridge, config);
    draw_waiting_queues(bridge);
    draw_legend();

    if (bridge->mode == MODE_POLICE) {
        draw_police_ui(bridge);
    }

    // Draw title
    draw_title("BRIDGE TRAFFIC SIMULATION", 500, 20);

    SDL_RenderPresent(renderer);
}

/* -------- GUI THREAD LOOP -------- */

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

        gui_render(bridge, config);  // Ahora pasa config a render
        SDL_Delay(33); // ~30 FPS
    }

    gui_destroy();
    return NULL;
}


/* -------- DESTROY -------- */

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