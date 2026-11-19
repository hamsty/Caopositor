#ifndef MIDI_CONTROLLER_H
#define MIDI_CONTROLLER_H

#include <Arduino.h>

void midi_init();
void midi_update();
void midi_handle_key(uint8_t code, uint8_t note);

#endif
