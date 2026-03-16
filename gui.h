#ifndef GUI_H
#define GUI_H

#include "bridge.h"

void gui_init();
void gui_render(Bridge* bridge);
void gui_destroy();
void* gui_run(void* arg);

#endif