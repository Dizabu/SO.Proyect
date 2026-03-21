#ifndef GUI_H
#define GUI_H

#include "bridge.h"
#include "config.h"

void gui_init();
void gui_render(Bridge* bridge, Config* config);
void* gui_run(Bridge* bridge, Config* config);  // Cambiado para aceptar config
void gui_destroy();

#endif