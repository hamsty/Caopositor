#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
#include "config.h"

void audio_init();
void audio_loop();

void audio_set_i2s_enabled(bool enabled);
bool audio_is_i2s_enabled();

void audio_handle_key(uint8_t code, uint8_t note, uint8_t slotId = 0);
void audio_stop_all();
void audio_volume_up();
void audio_volume_down();

int8_t *audio_get_sample_buffer(uint8_t slotId = 0);
int8_t *audio_get_record_buffer(); // Slots 1..3 contíguos (96 KB = 6s) para gravação longa
void audio_set_base_midi_note(float midiNote, uint8_t slotId = 0);
float audio_get_base_midi_note(uint8_t slotId = 0);
void audio_set_slot_length(uint32_t length, uint8_t slotId = 0);

// Buffer de amostras capturado em updateAudio() para o analisador de espectro
bool audio_is_spectrum_ready();
const volatile int *audio_get_spectrum_buffer();
void audio_release_spectrum_buffer();

#endif
