#ifndef UI_H
#define UI_H
#include <Arduino.h>

//inicia o display OLED
bool ui_init();
//desenha o menu principal
void ui_desenharMenu();
//atualiza a interface
void ui_update();
//informa a interface qual tecla fisica foi pressionada
void ui_onKeyPressed(int tecla);

#endif