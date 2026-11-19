#include "audio_engine.h"
#include "config.h"
#include <pico/stdlib.h>
#include <pico/util/queue.h>

#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_CONTROL_RATE AUDIO_CONTROL_RATE
#define MOZZI_I2S_PIN_BCK AUDIO_I2S_PIN_BCK
#define MOZZI_I2S_PIN_DATA AUDIO_I2S_PIN_DATA
#define MOZZI_AUDIO_RATE AUDIO_SAMPLE_RATE

#include <Mozzi.h>
#include <Sample.h>
#include <mozzi_midi.h>
#include <ADSR.h>

// 4 slots de 2 segundos (4 * 32 KB = 128 KB); slot 0 também é usado no Modo Teclado/Gravação
static int8_t sampleSlots[SEQ_NUM_SYMBOLS][TAMANHO_BUFFER];
static volatile float slotBaseMidiNote[SEQ_NUM_SYMBOLS] = {60.0f, 60.0f, 60.0f, 60.0f};
static volatile uint32_t slotSampleLength[SEQ_NUM_SYMBOLS] = {TAMANHO_BUFFER, TAMANHO_BUFFER, TAMANHO_BUFFER, TAMANHO_BUFFER};

static const float FREQ_BASE = (float)AUDIO_RATE / (float)TAMANHO_BUFFER;

static volatile byte volumeMestre = 255;
static volatile bool i2s_enable = false;

static Sample<TAMANHO_BUFFER, AUDIO_RATE> vozes[NUM_VOZES]{
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0]),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(sampleSlots[0])};

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

static bool vozes_ativas[NUM_VOZES] = {false};
static bool vozes_held[NUM_VOZES] = {false};
static uint8_t vozes_nota[NUM_VOZES] = {0};
static uint8_t vozes_slot[NUM_VOZES] = {0};
static uint32_t vozes_age[NUM_VOZES] = {0};
static uint32_t voiceCounter = 0;

static volatile int bufferAudioMatriz[AMOSTRAS_MATRIZ];
static volatile int indiceBufferMatriz = 0;
static volatile bool bufferMatrizPronto = false;

enum class AudioCmdType : uint8_t
{
  NOTE_ON,
  NOTE_OFF,
  STOP_ALL,
  SET_I2S
};

struct AudioCmd
{
  AudioCmdType type;
  uint8_t note;
  uint8_t slotId;
  uint8_t value;
};

static queue_t audioCmdQueue;

static void apply_note_on(uint8_t note, uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;

  int chosen = -1;

  // 1. Procura voz livre
  for (int i = 0; i < NUM_VOZES; i++)
  {
    if (!vozes_ativas[i])
    {
      chosen = i;
      break;
    }
  }

  // 2. Se todas estiverem ocupadas, rouba a voz em release mais antiga
  if (chosen == -1)
  {
    uint32_t oldestAge = 0xFFFFFFFF;
    for (int i = 0; i < NUM_VOZES; i++)
    {
      if (!vozes_held[i] && vozes_age[i] < oldestAge)
      {
        oldestAge = vozes_age[i];
        chosen = i;
      }
    }
  }

  // 3. Fallback: rouba a voz ativa mais antiga
  if (chosen == -1)
  {
    uint32_t oldestAge = 0xFFFFFFFF;
    for (int i = 0; i < NUM_VOZES; i++)
    {
      if (vozes_age[i] < oldestAge)
      {
        oldestAge = vozes_age[i];
        chosen = i;
      }
    }
  }

  if (chosen >= 0)
  {
    float baseNote = slotBaseMidiNote[slotId];
    float multiplicador = powf(2.0f, ((float)note - baseNote) / 12.0f);

    vozes[chosen].setTable(sampleSlots[slotId]);
    uint32_t endLen = slotSampleLength[slotId];
    if (endLen == 0 || endLen > TAMANHO_BUFFER)
      endLen = TAMANHO_BUFFER;
    vozes[chosen].setEnd(endLen);
    vozes[chosen].setFreq(FREQ_BASE * multiplicador);
    vozes[chosen].start();

    envelopes[chosen].noteOn(true);
    vozes_nota[chosen] = note;
    vozes_slot[chosen] = slotId;
    vozes_ativas[chosen] = true;
    vozes_held[chosen] = true;
    vozes_age[chosen] = ++voiceCounter;
  }
}

static void apply_note_off(uint8_t note, uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;

  for (int i = 0; i < NUM_VOZES; i++)
  {
    if (vozes_ativas[i] && vozes_held[i] && vozes_nota[i] == note && vozes_slot[i] == slotId)
    {
      vozes_held[i] = false;
      envelopes[i].noteOff();
      break;
    }
  }
}

static void apply_stop_all()
{
  for (int i = 0; i < NUM_VOZES; i++)
  {
    vozes_ativas[i] = false;
    vozes_held[i] = false;
    vozes_nota[i] = 0;
    envelopes[i].noteOff();
  }
}

static void apply_set_i2s(bool enabled)
{
  if (enabled && !i2s_enable)
  {
    MozziPrivate::i2s.begin();
    i2s_enable = true;
  }
  else if (!enabled && i2s_enable)
  {
    apply_stop_all();
    MozziPrivate::i2s.end();
    i2s_enable = false;
  }
}

static void process_audio_commands()
{
  AudioCmd cmd;
  while (queue_try_remove(&audioCmdQueue, &cmd))
  {
    switch (cmd.type)
    {
    case AudioCmdType::NOTE_ON:
      apply_note_on(cmd.note, cmd.slotId);
      break;
    case AudioCmdType::NOTE_OFF:
      apply_note_off(cmd.note, cmd.slotId);
      break;
    case AudioCmdType::STOP_ALL:
      apply_stop_all();
      break;
    case AudioCmdType::SET_I2S:
      apply_set_i2s(cmd.value != 0);
      break;
    }
  }
}

void audio_init()
{
  queue_init(&audioCmdQueue, sizeof(AudioCmd), 64);

  for (int i = 0; i < NUM_VOZES; i++)
  {
    envelopes[i].setADLevels(255, 128);
    envelopes[i].setTimes(30, 180, 10000, 160);
  }

  startMozzi();
  i2s_enable = true;

  Serial.println("Mozzi Started");
}

void audio_loop()
{
  process_audio_commands();
  if (i2s_enable)
  {
    audioHook();
  }
  else
  {
    tight_loop_contents();
  }
}

void audio_set_i2s_enabled(bool enabled)
{
  if (get_core_num() == 0)
  {
    apply_set_i2s(enabled);
    return;
  }

  AudioCmd cmd = {AudioCmdType::SET_I2S, 0, 0, enabled ? (uint8_t)1 : (uint8_t)0};
  queue_add_blocking(&audioCmdQueue, &cmd);

  // Aguarda confirmação do Core 0 para evitar race condition com SD/gravação
  unsigned long startWait = millis();
  while (i2s_enable != enabled && (millis() - startWait < 100))
  {
    delay(1);
  }
}

bool audio_is_i2s_enabled()
{
  return i2s_enable;
}

void audio_volume_up()
{
  if (volumeMestre <= 245)
  {
    volumeMestre += 10;
  }
  else
  {
    volumeMestre = 255;
  }
  Serial.print("Volume Up! Volume: ");
  Serial.println(volumeMestre);
}

void audio_volume_down()
{
  if (volumeMestre >= 10)
  {
    volumeMestre -= 10;
  }
  else
  {
    volumeMestre = 0;
  }
  Serial.print("Volume Down! Volume: ");
  Serial.println(volumeMestre);
}

uint8_t audio_get_volume()
{
  return volumeMestre;
}

void audio_handle_key(uint8_t code, uint8_t note, uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;

  if (get_core_num() == 0)
  {
    if (code == 0x90)
      apply_note_on(note, slotId);
    else if (code == 0x80)
      apply_note_off(note, slotId);
    return;
  }

  AudioCmd cmd;
  if (code == 0x90)
  {
    cmd = {AudioCmdType::NOTE_ON, note, slotId, 0};
    queue_try_add(&audioCmdQueue, &cmd);
  }
  else if (code == 0x80)
  {
    cmd = {AudioCmdType::NOTE_OFF, note, slotId, 0};
    queue_try_add(&audioCmdQueue, &cmd);
  }
}

void audio_stop_all()
{
  if (get_core_num() == 0)
  {
    apply_stop_all();
    return;
  }

  AudioCmd cmd = {AudioCmdType::STOP_ALL, 0, 0, 0};
  queue_try_add(&audioCmdQueue, &cmd);
}

int8_t *audio_get_sample_buffer(uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;
  return sampleSlots[slotId];
}

int8_t *audio_get_record_buffer()
{
  // Reutiliza os 3 slots superiores (slots 1, 2 e 3 = 96 KB contíguos = 6 segundos)
  return &sampleSlots[1][0];
}

void audio_set_base_midi_note(float midiNote, uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;
  slotBaseMidiNote[slotId] = midiNote;
}

float audio_get_base_midi_note(uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;
  return slotBaseMidiNote[slotId];
}

void audio_set_slot_length(uint32_t length, uint8_t slotId)
{
  if (slotId >= SEQ_NUM_SYMBOLS)
    slotId = 0;
  if (length == 0 || length > TAMANHO_BUFFER)
    length = TAMANHO_BUFFER;
  slotSampleLength[slotId] = length;
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
    if (vozes_ativas[i])
    {
      envelopes[i].update();
      if (!envelopes[i].playing() || !vozes[i].isPlaying())
      {
        vozes_ativas[i] = false;
        vozes_held[i] = false;
      }
    }
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
