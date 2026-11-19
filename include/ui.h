#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include "app_types.h"

// inicia o display OLED
bool ui_init();
// desenha o menu principal
void ui_desenharMenu();
// atualiza a interface
void ui_update();

Tela getCurrentScreen();

const char *getCurrentInstrumentFile();

int getCurrentInstrumentIndex();

void setInstrumentList(const char **instrumentList, int quantidade);

void setCurrentScreen(Tela novaTela);

void desenharSomGravado();

#endif
