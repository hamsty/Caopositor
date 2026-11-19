#ifndef RECORDER_H
#define RECORDER_H

#include <Arduino.h>

typedef void (*RecordingFinishedCallback)();

void recorder_init(int8_t *targetBuffer, RecordingFinishedCallback onFinished);
void recorder_start();
bool recorder_is_recording();

uint32_t recorder_get_sample_index();
volatile uint32_t *recorder_get_sample_index_ptr();

#endif
