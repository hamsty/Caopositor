#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "app_types.h"

bool storage_init();
void storage_scan_samples();
bool storage_load_sample(const char *nomeArquivo, int8_t *destBuffer, volatile uint32_t *outLength);
bool storage_save_wav(const char *nomeArquivo, const int8_t *srcBuffer, uint32_t length);
void storage_test_spi();

const char **storage_get_sample_list();
int storage_get_sample_count();
int storage_next_recorded_index();

// Persistência de músicas do sequenciador (.cseq)
void storage_scan_songs();
int storage_get_song_count();
const char *storage_get_song_name(int index);
int storage_next_song_index();
bool storage_save_song(const char *nomeArquivo, const SeqSong &song);
bool storage_load_song(const char *nomeArquivo, SeqSong &song);

#endif
