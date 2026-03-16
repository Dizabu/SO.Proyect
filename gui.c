#include <SDL2/SDL.h>
#include "gui.h"
#include "vehicle.h"
#include "bridge.h"

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

void gui_init()
{
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(
        "Bridge Simulation",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1200,
        400,
        0
    );

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

/* -------- DIBUJAR SEMAFOROS -------- */

void draw_traffic_lights(Bridge* bridge)
{
    int west_x = 70;
    int east_x = 100 + bridge->length * 10 + 10;
    int y = 160;

    SDL_Rect west_light = {west_x, y, 20, 20};
    SDL_Rect east_light = {east_x, y, 20, 20};

    if (bridge->light_direction == EAST)
    {
        // EAST verde
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &east_light);

        // WEST rojo
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &west_light);
    }
    else
    {
        // WEST verde
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &west_light);

        // EAST rojo
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &east_light);
    }
}

/* -------- RENDER -------- */

void gui_render(Bridge* bridge)
{
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    int segment_width = 10;
    int start_x = 100;

    int lane_west_y = 170;
    int lane_east_y = 210;

    for (int i = 0; i < bridge->length; i++)
    {
        int x = start_x + i * segment_width;

        // carretera
        SDL_Rect road = {x, 180, segment_width - 1, 40};

        SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
        SDL_RenderFillRect(renderer, &road);

        Vehicle* v = bridge->segment_vehicles[i];

        if (v != NULL)
        {
            int y;

            if (v->dir == WEST)
                y = lane_west_y;
            else
                y = lane_east_y;

            if (v->type == VEHICLE_AMBULANCE)
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            else if (v->dir == WEST)
                SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
            else
                SDL_SetRenderDrawColor(renderer, 0, 120, 255, 255);

            SDL_Rect car = {x, y, segment_width - 2, 12};

            SDL_RenderFillRect(renderer, &car);
        }
    }

    /* -------- SEMAFOROS SOLO SI EL MODO LO USA -------- */

    if (bridge->mode == MODE_SEMAFOROS)
    {
        draw_traffic_lights(bridge);
    }

    SDL_RenderPresent(renderer);
}

/* -------- GUI LOOP -------- */

void* gui_run(void* arg)
{
    Bridge* bridge = (Bridge*) arg;

    gui_init();

    SDL_Event e;
    int running = 1;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        gui_render(bridge);

        SDL_Delay(30);
    }

    gui_destroy();

    return NULL;
}

/* -------- DESTROY -------- */

void gui_destroy()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}