#ifndef RECORDER_H
#define RECORDER_H

#include <Arduino.h>

void recorder_init(int8_t *targetBuffer, uint32_t maxSamples);
void recorder_start();
void recorder_stop();
void recorder_cancel();

bool recorder_is_recording();
bool recorder_consume_finished();

int8_t *recorder_get_buffer();
uint32_t recorder_get_sample_index();
uint32_t recorder_get_max_samples();
volatile uint32_t *recorder_get_sample_index_ptr();

#endif
