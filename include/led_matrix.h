#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include <Arduino.h>
#include "app_types.h"

void led_matrix_init();
void led_matrix_trigger_firework();
void led_matrix_update(Tela telaAtual);

#endif
