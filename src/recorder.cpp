#include "recorder.h"
#include "config.h"
#include "audio_engine.h"
#include "sequencer.h"
#include "pico/stdlib.h"

static struct repeating_timer timerGravacao;
static int8_t *sampleBufferPtr = nullptr;
static uint32_t maxSampleCount = RECORD_MAX_SAMPLES;

static volatile bool gravando = false;
static volatile bool gravacaoConcluida = false;
static volatile uint32_t loadedSampleMaxIndex = 0;

static bool rotinaDeGravacao(struct repeating_timer *t)
{
  (void)t;

  if (gravando && sampleBufferPtr != nullptr && loadedSampleMaxIndex < maxSampleCount)
  {
    int leituraADC = analogRead(PINO_MICROFONE);

    // Desloca 4 bits à direita (12 -> 8 bits) e centraliza no zero
    sampleBufferPtr[loadedSampleMaxIndex] = (leituraADC >> 4) - 128;

    loadedSampleMaxIndex++;
    if (loadedSampleMaxIndex >= maxSampleCount)
    {
      gravando = false;
      gravacaoConcluida = true;
    }
  }
  return true;
}

void recorder_init(int8_t *targetBuffer, uint32_t maxSamples)
{
  sampleBufferPtr = targetBuffer;
  maxSampleCount = (maxSamples > 0) ? maxSamples : TAMANHO_BUFFER;
  gravando = false;
  gravacaoConcluida = false;
  loadedSampleMaxIndex = 0;
  add_repeating_timer_us(-61, rotinaDeGravacao, NULL, &timerGravacao);
}

void recorder_start()
{
  if (audio_is_i2s_enabled())
  {
    audio_set_i2s_enabled(false);
  }
  sequencer_mark_all_slots_dirty();
  gravacaoConcluida = false;
  loadedSampleMaxIndex = 0;
  gravando = true;
}

void recorder_stop()
{
  if (gravando)
  {
    gravando = false;
    if (loadedSampleMaxIndex > 0)
    {
      gravacaoConcluida = true;
    }
  }
}

void recorder_cancel()
{
  gravando = false;
  gravacaoConcluida = false;
}

bool recorder_is_recording()
{
  return gravando;
}

bool recorder_consume_finished()
{
  if (gravacaoConcluida)
  {
    gravacaoConcluida = false;
    return true;
  }
  return false;
}

int8_t *recorder_get_buffer()
{
  return sampleBufferPtr;
}

uint32_t recorder_get_sample_index()
{
  return loadedSampleMaxIndex;
}

uint32_t recorder_get_max_samples()
{
  return maxSampleCount;
}

volatile uint32_t *recorder_get_sample_index_ptr()
{
  return &loadedSampleMaxIndex;
}
