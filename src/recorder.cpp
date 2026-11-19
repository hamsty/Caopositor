#include "recorder.h"
#include "config.h"
#include "audio_engine.h"
#include "pico/stdlib.h" // Biblioteca nativa do RP2040 para timers

static struct repeating_timer timerGravacao;
static int8_t *sampleBufferPtr = nullptr;
static RecordingFinishedCallback finishedCallback = nullptr;

static volatile bool gravando = false;
static volatile uint32_t loadedSampleMaxIndex = 0;

static bool rotinaDeGravacao(struct repeating_timer *t)
{
  (void)t;

  if (gravando && sampleBufferPtr != nullptr && loadedSampleMaxIndex < TAMANHO_BUFFER)
  {
    int leituraADC = analogRead(PINO_MICROFONE);

    // Desloca 4 bits à direita (divide por 16) para ir de 12 para 8 bits, depois centraliza no zero
    sampleBufferPtr[loadedSampleMaxIndex] = (leituraADC >> 4) - 128;

    loadedSampleMaxIndex++;
    if (loadedSampleMaxIndex == TAMANHO_BUFFER)
    {
      if (finishedCallback != nullptr)
      {
        finishedCallback();
      }
    }
  }
  else
  {
    gravando = false;
  }
  return true; // Mantém o temporizador ativo em segundo plano
}

void recorder_init(int8_t *targetBuffer, RecordingFinishedCallback onFinished)
{
  sampleBufferPtr = targetBuffer;
  finishedCallback = onFinished;
  add_repeating_timer_us(-61, rotinaDeGravacao, NULL, &timerGravacao);
}

void recorder_start()
{
  loadedSampleMaxIndex = 0;
  gravando = true;
  if (audio_is_i2s_enabled())
  {
    audio_set_i2s_enabled(false);
  }
}

bool recorder_is_recording()
{
  return gravando;
}

uint32_t recorder_get_sample_index()
{
  return loadedSampleMaxIndex;
}

volatile uint32_t *recorder_get_sample_index_ptr()
{
  return &loadedSampleMaxIndex;
}
