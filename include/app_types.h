#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <Arduino.h>
#include "config.h"

enum class Tela {
    MENU,
    MIDI,
    TECLADO,
    GRAVAR,
    GRAVANDO,
    EDITAR_GRAVACAO,
    SOM_GRAVADO,
    COMPOSITOR,
    SEQ_CONFIG,
    SEQ_SIMBOLOS,
    SEQ_ESCOLHER_SAMPLE,
    SEQ_ARQUIVOS
};

struct SeqNote {
    uint8_t midiNote; // 0 = vazio; 36..96 = nota MIDI
    uint8_t symbolId; // 0..(SEQ_NUM_SYMBOLS - 1)
    uint8_t duration; // Duração em slots de tempo (1 = 1 slot / colcheia, 2 = 2 slots, etc.)
};

struct SeqSong {
    uint16_t bpm;
    uint8_t beatsPerBar; // 3 (3/4) ou 4 (4/4)
    uint8_t numBars;     // 2, 4 ou 8 compassos
    char symbolFiles[SEQ_NUM_SYMBOLS][SEQ_FILENAME_LEN]; // Nome sem ".wav"
    SeqNote steps[SEQ_MAX_STEPS][SEQ_MAX_NOTES_PER_STEP];
};

#endif
