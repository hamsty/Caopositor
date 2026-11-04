#ifndef UI_H
#define UI_H
#include <Arduino.h>

enum class Tela {
    MENU,
    MIDI,
    TECLADO,
    GRAVAR,
    GRAVANDO,
    SOM_GRAVADO
};

//inicia o display OLED
bool ui_init();
//desenha o menu principal
void ui_desenharMenu();
//atualiza a interface
void ui_update();
// //informa a interface qual tecla fisica foi pressionada
// void ui_onKeyPressed(int tecla);

Tela getCurrentScreen();

const char* getCurrentInstrumentFile();

int getCurrentInstrumentIndex();

void setInstrumentList(const char** instrumentList, int quantidade);

void setCurrentScreen(Tela novaTela);

void desenharSomGravado();

#endif