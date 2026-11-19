#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <Arduino.h>
#include "app_types.h"
#include "config.h"

#define SEQ_STAFF_POSITIONS 23 // F2 (idx 0, y=56) até G5 (idx 22, y=12)

void sequencer_init();
bool sequencer_update(); // Retorna true quando avança de passo (para atualizar OLED)

// Controle de reprodução
void sequencer_play();
void sequencer_stop();
void sequencer_toggle_play();
bool sequencer_is_playing();
uint8_t sequencer_get_play_step();

// Edição da partitura
bool sequencer_add_note(uint8_t step, uint8_t midiNote, uint8_t symbolId, uint8_t duration = 1);
void sequencer_set_note_duration(uint8_t step, uint8_t midiNote, uint8_t symbolId, uint8_t duration);
bool sequencer_toggle_note(uint8_t step, uint8_t midiNote, uint8_t symbolId);
void sequencer_clear_step(uint8_t step);
void sequencer_clear_song();
uint8_t sequencer_get_total_steps();
uint8_t sequencer_get_steps_per_bar();
uint32_t sequencer_get_step_duration_ms();

// Parâmetros musicais
void sequencer_set_bpm(uint16_t bpm);
void sequencer_change_bpm(int delta);
uint16_t sequencer_get_bpm();
void sequencer_toggle_time_signature(); // Alterna 4/4 <-> 3/4
uint8_t sequencer_get_beats_per_bar();
void sequencer_cycle_num_bars(); // Alterna 2 -> 4 -> 8 compassos
uint8_t sequencer_get_num_bars();

// Gerenciamento de símbolos e banco de samples (4 slots de 2s)
void sequencer_mark_slot0_dirty();
void sequencer_mark_all_slots_dirty();
void sequencer_ensure_samples_loaded();
bool sequencer_assign_symbol_sample(uint8_t symbolId, const char *sampleNameNoExt);
const char *sequencer_get_symbol_sample_name(uint8_t symbolId);
void sequencer_preview_note(uint8_t midiNote, uint8_t symbolId);

// Salvar / Carregar músicas (.cseq) no SD
bool sequencer_save_current_song(char *outFileName = nullptr, size_t outSize = 0);
bool sequencer_load_song_by_index(int songIndex);

// Acesso somente-leitura à partitura para a UI
const SeqSong &sequencer_get_song();

// Mapeamento musical Clave de Sol / Clave de Fá
uint8_t sequencer_staff_index_to_midi(int staffIndex);
int sequencer_staff_index_to_y(int staffIndex);
void sequencer_midi_to_staff_y(uint8_t midiNote, int &outY, bool &outSharp);
const char *sequencer_midi_to_note_name(uint8_t midiNote, char *buf, size_t bufSize);
const char *sequencer_midi_to_compact_dual_name(uint8_t midiNote, char *buf, size_t bufSize);
const char *sequencer_midi_to_full_dual_name(uint8_t midiNote, char *buf, size_t bufSize);

#endif
