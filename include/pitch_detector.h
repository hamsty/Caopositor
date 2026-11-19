#ifndef PITCH_DETECTOR_H
#define PITCH_DETECTOR_H

#include <Arduino.h>
#include "config.h"

// Analisa o buffer de áudio e retorna a nota MIDI fracionária dominante
float pitch_detect_midi_note(const int8_t *sampleBuffer, uint32_t sampleLength = TAMANHO_BUFFER);

#endif
