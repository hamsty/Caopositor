
#ifndef UI_H
#define UI_H

#include <Arduino.h>

// Inicializa o display OLED
bool ui_init();

// Desenha o menu principal
void ui_desenharMenu();

// Atualiza a interface quando necessário
void ui_update();

#endif