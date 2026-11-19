#include "audio_engine.h"
#include "config.h"

#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_CONTROL_RATE AUDIO_CONTROL_RATE
#define MOZZI_I2S_PIN_BCK AUDIO_I2S_PIN_BCK
#define MOZZI_I2S_PIN_DATA AUDIO_I2S_PIN_DATA
#define MOZZI_AUDIO_RATE AUDIO_SAMPLE_RATE

#include <Mozzi.h>
#include <Sample.h>
#include <mozzi_midi.h>
#include <ADSR.h>

static int8_t loadedSample[TAMANHO_BUFFER];
static const float FREQ_BASE = (float)AUDIO_RATE / (float)TAMANHO_BUFFER;

static volatile byte volumeMestre = 255;
static volatile float notaOriginalGravada = 60.0;
static bool i2s_enable = false;

static Sample<TAMANHO_BUFFER, AUDIO_RATE> vozes[NUM_VOZES]{
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample)};

static ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelopes[NUM_VOZES]{
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>(),
    ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE>()};

static bool vozes_ativas[NUM_VOZES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t vozes_nota[NUM_VOZES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static volatile int bufferAudioMatriz[AMOSTRAS_MATRIZ];
static volatile int indiceBufferMatriz = 0;
static volatile bool bufferMatrizPronto = false;

void audio_init()
{
  for (int i = 0; i < NUM_VOZES; i++)
  {
    envelopes[i].setADLevels(255, 64);
    envelopes[i].setTimes(50, 200, 10000, 200);
  }

  startMozzi();
  i2s_enable = true;

  Serial.println("Mozzi Started");
}

void audio_loop()
{
  audioHook();
}

void audio_set_i2s_enabled(bool enabled)
{
  if (enabled && !i2s_enable)
  {
    MozziPrivate::i2s.begin();
    i2s_enable = true;
  }
  else if (!enabled && i2s_enable)
  {
    MozziPrivate::i2s.end();
    i2s_enable = false;
  }
}

bool audio_is_i2s_enabled()
{
  return i2s_enable;
}

void audio_volume_up()
{
  if (volumeMestre < 255)
  {
    volumeMestre += 10;
    Serial.print("Volume Up! Volume: ");
    Serial.println(volumeMestre);
  }
}

void audio_volume_down()
{
  if (volumeMestre > 10)
  {
    volumeMestre -= 10;
    Serial.print("Volume Down! Volume: ");
    Serial.println(volumeMestre);
  }
}

void audio_handle_key(uint8_t code, uint8_t note)
{
  switch (code)
  {
  case 0x90:
    for (int i = 0; i < NUM_VOZES; i++)
    {
      if (!vozes_ativas[i])
      {
        // Calcula o deslocamento de tom (pitch) relativo à nota do som original gravado
        float multiplicador = pow(2.0, (note - notaOriginalGravada) / 12.0);
        vozes[i].setFreq(FREQ_BASE * multiplicador);

        vozes[i].start(); // Reinicia o ponteiro de leitura do sample para o início
        envelopes[i].noteOn();
        vozes_nota[i] = note;
        vozes_ativas[i] = true;
        break;
      }
      if (i == NUM_VOZES - 1)
      {
        Serial.println("Todas as vozes estão ocupadas!");
        return;
      }
    }
    break;

  case 0x80:
    for (int i = 0; i < NUM_VOZES; i++)
    {
      if (vozes_ativas[i] && vozes_nota[i] == note)
      {
        vozes_ativas[i] = false;
        envelopes[i].noteOff();
        break;
      }
    }
    break;

  default:
    break;
  }
}

int8_t *audio_get_sample_buffer()
{
  return loadedSample;
}

void audio_set_base_midi_note(float midiNote)
{
  notaOriginalGravada = midiNote;
}

float audio_get_base_midi_note()
{
  return notaOriginalGravada;
}

bool audio_is_spectrum_ready()
{
  return bufferMatrizPronto;
}

const volatile int *audio_get_spectrum_buffer()
{
  return bufferAudioMatriz;
}

void audio_release_spectrum_buffer()
{
  bufferMatrizPronto = false;
}

void updateControl()
{
  for (int i = 0; i < NUM_VOZES; i++)
  {
    envelopes[i].update();
  }
}

AudioOutput updateAudio()
{
  int32_t mix = 0;
  for (int i = 0; i < NUM_VOZES; i++)
  {
    if (vozes_ativas[i])
    {
      mix += (envelopes[i].next() * vozes[i].next());
    }
  }

  mix = (mix * volumeMestre) >> 8;

  if (!bufferMatrizPronto)
  {
    bufferAudioMatriz[indiceBufferMatriz] = mix / 4;
    indiceBufferMatriz++;

    if (indiceBufferMatriz >= AMOSTRAS_MATRIZ)
    {
      bufferMatrizPronto = true;
      indiceBufferMatriz = 0;
    }
  }

  return MonoOutput::from16Bit(mix / 4);
}
