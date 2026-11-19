#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>

typedef void (*KeyNoteCallback)(uint8_t code, uint8_t note);
typedef void (*KeyControlCallback)(uint8_t key);

void keyboard_init(KeyNoteCallback onNote, KeyControlCallback onControl);
void keyboard_loop();
void keyboard_octave_up();
void keyboard_octave_down();

#endif
