#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>

void audio_init();
void audio_loop();

void audio_set_i2s_enabled(bool enabled);
bool audio_is_i2s_enabled();

void audio_handle_key(uint8_t code, uint8_t note);
void audio_volume_up();
void audio_volume_down();

int8_t *audio_get_sample_buffer();
void audio_set_base_midi_note(float midiNote);
float audio_get_base_midi_note();

// Buffer de amostras capturado em updateAudio() para o analisador de espectro
bool audio_is_spectrum_ready();
const volatile int *audio_get_spectrum_buffer();
void audio_release_spectrum_buffer();

#endif
