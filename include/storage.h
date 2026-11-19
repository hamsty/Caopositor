#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

bool storage_init();
void storage_scan_samples();
bool storage_load_sample(const char *nomeArquivo, int8_t *destBuffer, volatile uint32_t *outLength);
bool storage_save_wav(const char *nomeArquivo, const int8_t *srcBuffer, uint32_t length);
void storage_test_spi();

const char **storage_get_sample_list();
int storage_get_sample_count();
int storage_next_recorded_index();

#endif
