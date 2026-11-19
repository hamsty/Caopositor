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
// redesenha a tela atual (ex.: quando o playhead do sequenciador avança)
void ui_redraw_current();
// exibe tela rápida de carregamento
void ui_show_loading(const char *msg);

Tela getCurrentScreen();

const char *getCurrentInstrumentFile();

int getCurrentInstrumentIndex();

void setInstrumentList(const char **instrumentList, int quantidade);

void setCurrentScreen(Tela novaTela);

void desenharSomGravado();
void ui_open_recording_editor();

// Integração do teclado físico com o compositor e exibição de notas
void ui_on_composer_piano_key(uint8_t code, uint8_t note);
void ui_on_composer_rest_key();
void ui_on_keyboard_note_played(uint8_t note);
void ui_show_volume_popup(uint8_t volume);
uint8_t ui_get_active_symbol();

#endif
