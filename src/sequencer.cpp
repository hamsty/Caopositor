#include "sequencer.h"
#include "audio_engine.h"
#include "pitch_detector.h"
#include "storage.h"
#include <string.h>

static SeqSong currentSong;
static bool isPlaying = false;
static uint8_t currentPlayStep = 0;
static uint32_t nextStepTimeUs = 0;

// Controle de notas ativas com sustentação por múltiplos slots
struct ActiveSeqVoice
{
  bool active;
  uint8_t midiNote;
  uint8_t symbolId;
  uint8_t remainingSteps;
};

static ActiveSeqVoice activeVoices[NUM_VOZES];

// Controle de carregamento dos 4 slots de 2s na RAM
static bool slotLoaded[SEQ_NUM_SYMBOLS] = {false, false, false, false};

// Preview temporizado (para previews disparados pelo joystick/configuração)
static bool previewActive = false;
static uint8_t previewNote = 0;
static uint8_t previewSymbol = 0;
static unsigned long previewEndMs = 0;

// Tabela diatônica exata da pauta dupla: F2 (index 0, y=56) até G5 (index 22, y=12)
static const uint8_t STAFF_DIATONIC_MIDI[SEQ_STAFF_POSITIONS] = {
    41, // 0:  F2 (y=56)
    43, // 1:  G2 (y=54) - Clave de Fá L1
    45, // 2:  A2 (y=52)
    47, // 3:  B2 (y=50) - Clave de Fá L2
    48, // 4:  C3 (y=48)
    50, // 5:  D3 (y=46) - Clave de Fá L3
    52, // 6:  E3 (y=44)
    53, // 7:  F3 (y=42) - Clave de Fá L4
    55, // 8:  G3 (y=40)
    57, // 9:  A3 (y=38) - Clave de Fá L5
    59, // 10: B3 (y=36)
    60, // 11: C4 (y=34) - Dó Central (linha suplementar)
    62, // 12: D4 (y=32)
    64, // 13: E4 (y=30) - Clave de Sol L1
    65, // 14: F4 (y=28)
    67, // 15: G4 (y=26) - Clave de Sol L2
    69, // 16: A4 (y=24)
    71, // 17: B4 (y=22) - Clave de Sol L3
    72, // 18: C5 (y=20)
    74, // 19: D5 (y=18) - Clave de Sol L4
    76, // 20: E5 (y=16)
    77, // 21: F5 (y=14) - Clave de Sol L5
    79  // 22: G5 (y=12)
};

static uint32_t getStepDurationUs()
{
  // Cada passo equivale a uma colcheia (2 passos por tempo)
  uint16_t bpm = currentSong.bpm;
  if (bpm < SEQ_MIN_BPM)
    bpm = SEQ_MIN_BPM;
  return 30000000UL / (uint32_t)bpm;
}

static void releaseAllActiveVoices()
{
  for (uint8_t i = 0; i < NUM_VOZES; i++)
  {
    if (activeVoices[i].active)
    {
      audio_handle_key(0x80, activeVoices[i].midiNote, activeVoices[i].symbolId);
      activeVoices[i].active = false;
      activeVoices[i].remainingSteps = 0;
    }
  }
}

static void advanceAndReleaseExpiredVoices()
{
  for (uint8_t i = 0; i < NUM_VOZES; i++)
  {
    if (activeVoices[i].active)
    {
      if (activeVoices[i].remainingSteps > 0)
      {
        activeVoices[i].remainingSteps--;
      }
      if (activeVoices[i].remainingSteps == 0)
      {
        audio_handle_key(0x80, activeVoices[i].midiNote, activeVoices[i].symbolId);
        activeVoices[i].active = false;
      }
    }
  }
}

static void trackActiveVoice(uint8_t midiNote, uint8_t symbolId, uint8_t duration)
{
  if (duration < 1)
    duration = 1;

  // Se já existe a mesma nota/símbolo sustentando, libera antes de reatacar
  int freeSlot = -1;
  for (uint8_t i = 0; i < NUM_VOZES; i++)
  {
    if (activeVoices[i].active && activeVoices[i].midiNote == midiNote && activeVoices[i].symbolId == symbolId)
    {
      audio_handle_key(0x80, midiNote, symbolId);
      activeVoices[i].active = false;
      freeSlot = i;
      break;
    }
    if (!activeVoices[i].active && freeSlot == -1)
    {
      freeSlot = i;
    }
  }

  // Fallback: substitui a voz com menor tempo restante
  if (freeSlot == -1)
  {
    uint8_t minRem = 255;
    for (uint8_t i = 0; i < NUM_VOZES; i++)
    {
      if (activeVoices[i].remainingSteps < minRem)
      {
        minRem = activeVoices[i].remainingSteps;
        freeSlot = i;
      }
    }
    if (freeSlot >= 0)
    {
      audio_handle_key(0x80, activeVoices[freeSlot].midiNote, activeVoices[freeSlot].symbolId);
    }
  }

  if (freeSlot >= 0)
  {
    activeVoices[freeSlot].active = true;
    activeVoices[freeSlot].midiNote = midiNote;
    activeVoices[freeSlot].symbolId = symbolId;
    activeVoices[freeSlot].remainingSteps = duration;
  }
}

static void triggerStepNotes(uint8_t step, bool advanceExisting)
{
  if (advanceExisting)
  {
    advanceAndReleaseExpiredVoices();
  }
  else
  {
    releaseAllActiveVoices();
  }

  if (step >= SEQ_MAX_STEPS)
    return;

  for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
  {
    const SeqNote &n = currentSong.steps[step][i];
    if (n.midiNote != 0 && n.symbolId < SEQ_NUM_SYMBOLS)
    {
      uint8_t dur = (n.duration >= 1) ? n.duration : 1;
      trackActiveVoice(n.midiNote, n.symbolId, dur);
      audio_handle_key(0x90, n.midiNote, n.symbolId);
    }
  }
}

void sequencer_init()
{
  currentSong.bpm = SEQ_DEFAULT_BPM;
  currentSong.beatsPerBar = 4;
  currentSong.numBars = 4;
  memset(currentSong.steps, 0, sizeof(currentSong.steps));

  for (int i = 0; i < SEQ_NUM_SYMBOLS; i++)
  {
    currentSong.symbolFiles[i][0] = '\0';
    slotLoaded[i] = false;
  }

  isPlaying = false;
  currentPlayStep = 0;
  memset(activeVoices, 0, sizeof(activeVoices));
  previewActive = false;
}

void sequencer_mark_slot0_dirty()
{
  slotLoaded[0] = false;
}

void sequencer_mark_all_slots_dirty()
{
  for (int i = 0; i < SEQ_NUM_SYMBOLS; i++)
  {
    slotLoaded[i] = false;
  }
}

bool sequencer_assign_symbol_sample(uint8_t symbolId, const char *sampleNameNoExt)
{
  if (symbolId >= SEQ_NUM_SYMBOLS || sampleNameNoExt == nullptr || sampleNameNoExt[0] == '\0')
  {
    return false;
  }

  strncpy(currentSong.symbolFiles[symbolId], sampleNameNoExt, SEQ_FILENAME_LEN - 1);
  currentSong.symbolFiles[symbolId][SEQ_FILENAME_LEN - 1] = '\0';

  char fullPath[SEQ_FILENAME_LEN + 8];
  snprintf(fullPath, sizeof(fullPath), "%s.wav", currentSong.symbolFiles[symbolId]);

  bool wasI2sEnabled = audio_is_i2s_enabled();
  audio_set_i2s_enabled(false);

  int8_t *slotBuf = audio_get_sample_buffer(symbolId);
  volatile uint32_t loadedLen = 0;
  bool ok = storage_load_sample(fullPath, slotBuf, &loadedLen);

  if (ok)
  {
    audio_set_slot_length(loadedLen, symbolId);
    float detectedMidi = pitch_detect_midi_note(slotBuf, loadedLen);
    audio_set_base_midi_note(detectedMidi, symbolId);
    slotLoaded[symbolId] = true;
  }

  if (wasI2sEnabled)
  {
    audio_set_i2s_enabled(true);
  }

  return ok;
}

void sequencer_ensure_samples_loaded()
{
  int sampleCount = storage_get_sample_count();
  const char **sampleList = storage_get_sample_list();

  if (sampleCount <= 0 || sampleList == nullptr)
  {
    return;
  }

  bool needsLoad = false;
  for (int s = 0; s < SEQ_NUM_SYMBOLS; s++)
  {
    if (currentSong.symbolFiles[s][0] == '\0')
    {
      const char *defaultName = sampleList[s % sampleCount];
      strncpy(currentSong.symbolFiles[s], defaultName, SEQ_FILENAME_LEN - 1);
      currentSong.symbolFiles[s][SEQ_FILENAME_LEN - 1] = '\0';
      slotLoaded[s] = false;
    }
    if (!slotLoaded[s])
    {
      needsLoad = true;
    }
  }

  if (!needsLoad)
    return;

  bool wasI2sEnabled = audio_is_i2s_enabled();
  audio_set_i2s_enabled(false);

  for (int s = 0; s < SEQ_NUM_SYMBOLS; s++)
  {
    if (!slotLoaded[s] && currentSong.symbolFiles[s][0] != '\0')
    {
      char fullPath[SEQ_FILENAME_LEN + 8];
      snprintf(fullPath, sizeof(fullPath), "%s.wav", currentSong.symbolFiles[s]);

      int8_t *slotBuf = audio_get_sample_buffer(s);
      volatile uint32_t loadedLen = 0;
      if (storage_load_sample(fullPath, slotBuf, &loadedLen))
      {
        audio_set_slot_length(loadedLen, s);
        float detectedMidi = pitch_detect_midi_note(slotBuf, loadedLen);
        audio_set_base_midi_note(detectedMidi, s);
        slotLoaded[s] = true;
      }
      else
      {
        // Fallback para o primeiro sample disponível no SD se o arquivo salvo não existir
        const char *fallback = sampleList[s % sampleCount];
        strncpy(currentSong.symbolFiles[s], fallback, SEQ_FILENAME_LEN - 1);
        currentSong.symbolFiles[s][SEQ_FILENAME_LEN - 1] = '\0';
        snprintf(fullPath, sizeof(fullPath), "%s.wav", fallback);
        if (storage_load_sample(fullPath, slotBuf, &loadedLen))
        {
          audio_set_slot_length(loadedLen, s);
          float detectedMidi = pitch_detect_midi_note(slotBuf, loadedLen);
          audio_set_base_midi_note(detectedMidi, s);
          slotLoaded[s] = true;
        }
      }
    }
  }

  if (wasI2sEnabled)
  {
    audio_set_i2s_enabled(true);
  }
}

const char *sequencer_get_symbol_sample_name(uint8_t symbolId)
{
  if (symbolId >= SEQ_NUM_SYMBOLS)
    return "";
  if (currentSong.symbolFiles[symbolId][0] == '\0')
    return "(vazio)";
  return currentSong.symbolFiles[symbolId];
}

void sequencer_preview_note(uint8_t midiNote, uint8_t symbolId)
{
  if (symbolId >= SEQ_NUM_SYMBOLS)
    symbolId = 0;

  if (previewActive)
  {
    audio_handle_key(0x80, previewNote, previewSymbol);
  }

  audio_handle_key(0x90, midiNote, symbolId);
  previewActive = true;
  previewNote = midiNote;
  previewSymbol = symbolId;
  previewEndMs = millis() + 260;
}

void sequencer_play()
{
  if (isPlaying)
    return;

  if (!audio_is_i2s_enabled())
  {
    audio_set_i2s_enabled(true);
  }

  isPlaying = true;
  currentPlayStep = 0;
  triggerStepNotes(currentPlayStep, false);
  nextStepTimeUs = micros() + getStepDurationUs();
}

void sequencer_stop()
{
  if (!isPlaying)
    return;

  isPlaying = false;
  releaseAllActiveVoices();
  audio_stop_all();
}

void sequencer_toggle_play()
{
  if (isPlaying)
    sequencer_stop();
  else
    sequencer_play();
}

bool sequencer_is_playing()
{
  return isPlaying;
}

uint8_t sequencer_get_play_step()
{
  return currentPlayStep;
}

bool sequencer_update()
{
  // Finaliza preview curto se estiver ativo
  if (previewActive && (int32_t)(millis() - previewEndMs) >= 0)
  {
    audio_handle_key(0x80, previewNote, previewSymbol);
    previewActive = false;
  }

  if (!isPlaying)
  {
    return false;
  }

  uint32_t nowUs = micros();
  if ((int32_t)(nowUs - nextStepTimeUs) >= 0)
  {
    uint8_t totalSteps = sequencer_get_total_steps();
    currentPlayStep = (currentPlayStep + 1) % totalSteps;

    // Dispara áudio ANTES de retornar true para o OLED (minimiza jitter)
    triggerStepNotes(currentPlayStep, true);

    uint32_t durUs = getStepDurationUs();
    nextStepTimeUs += durUs;

    // Evita acúmulo caso algo tenha bloqueado por mais de 2 passos
    if ((int32_t)(nowUs - nextStepTimeUs) > (int32_t)(durUs * 2))
    {
      nextStepTimeUs = nowUs + durUs;
    }
    return true;
  }

  return false;
}

bool sequencer_add_note(uint8_t step, uint8_t midiNote, uint8_t symbolId, uint8_t duration)
{
  if (step >= SEQ_MAX_STEPS || midiNote == 0 || symbolId >= SEQ_NUM_SYMBOLS)
  {
    return false;
  }
  if (duration < 1)
    duration = 1;
  if (duration > SEQ_MAX_STEPS)
    duration = SEQ_MAX_STEPS;

  // Se já existe nota nessa altura, atualiza símbolo e duração (evita duplicata)
  for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
  {
    if (currentSong.steps[step][i].midiNote == midiNote)
    {
      currentSong.steps[step][i].symbolId = symbolId;
      currentSong.steps[step][i].duration = duration;
      return true;
    }
  }

  // Insere no primeiro slot livre do passo (permite acordes e múltiplas camadas)
  for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
  {
    if (currentSong.steps[step][i].midiNote == 0)
    {
      currentSong.steps[step][i].midiNote = midiNote;
      currentSong.steps[step][i].symbolId = symbolId;
      currentSong.steps[step][i].duration = duration;
      return true;
    }
  }

  return false; // Passo cheio
}

void sequencer_set_note_duration(uint8_t step, uint8_t midiNote, uint8_t symbolId, uint8_t duration)
{
  if (step >= SEQ_MAX_STEPS || midiNote == 0 || symbolId >= SEQ_NUM_SYMBOLS)
    return;
  if (duration < 1)
    duration = 1;
  if (duration > SEQ_MAX_STEPS)
    duration = SEQ_MAX_STEPS;

  for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
  {
    if (currentSong.steps[step][i].midiNote == midiNote && currentSong.steps[step][i].symbolId == symbolId)
    {
      currentSong.steps[step][i].duration = duration;
      return;
    }
  }
}

void sequencer_clear_step(uint8_t step)
{
  if (step >= SEQ_MAX_STEPS)
    return;

  // Apaga notas que começam nesse slot
  memset(currentSong.steps[step], 0, sizeof(currentSong.steps[step]));

  // Recorta sustentações de slots anteriores que atravessavam esse slot
  for (uint8_t s = 0; s < step; s++)
  {
    for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
    {
      SeqNote &n = currentSong.steps[s][i];
      if (n.midiNote != 0 && n.duration > 1 && (s + n.duration) > step)
      {
        n.duration = step - s;
      }
    }
  }
}

bool sequencer_toggle_note(uint8_t step, uint8_t midiNote, uint8_t symbolId)
{
  if (step >= SEQ_MAX_STEPS || midiNote == 0 || symbolId >= SEQ_NUM_SYMBOLS)
  {
    return false;
  }

  // 1. Verifica se já existe nota nessa altura
  for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
  {
    if (currentSong.steps[step][i].midiNote == midiNote)
    {
      if (currentSong.steps[step][i].symbolId == symbolId)
      {
        // Mesmo símbolo na mesma altura -> remove (toggle) e compacta
        for (uint8_t k = i; k < SEQ_MAX_NOTES_PER_STEP - 1; k++)
        {
          currentSong.steps[step][k] = currentSong.steps[step][k + 1];
        }
        currentSong.steps[step][SEQ_MAX_NOTES_PER_STEP - 1] = {0, 0, 0};
        return false;
      }
      else
      {
        // Outro símbolo na mesma altura -> substitui o símbolo
        currentSong.steps[step][i].symbolId = symbolId;
        if (currentSong.steps[step][i].duration < 1)
          currentSong.steps[step][i].duration = 1;
        return true;
      }
    }
  }

  // 2. Insere no primeiro slot livre do passo
  for (uint8_t i = 0; i < SEQ_MAX_NOTES_PER_STEP; i++)
  {
    if (currentSong.steps[step][i].midiNote == 0)
    {
      currentSong.steps[step][i].midiNote = midiNote;
      currentSong.steps[step][i].symbolId = symbolId;
      currentSong.steps[step][i].duration = 1;
      return true;
    }
  }

  return false; // Passo cheio
}

void sequencer_clear_song()
{
  sequencer_stop();
  memset(currentSong.steps, 0, sizeof(currentSong.steps));
  currentPlayStep = 0;
}

uint8_t sequencer_get_steps_per_bar()
{
  return currentSong.beatsPerBar * 2; // 8 em 4/4, 6 em 3/4
}

uint32_t sequencer_get_step_duration_ms()
{
  return getStepDurationUs() / 1000UL;
}

uint8_t sequencer_get_total_steps()
{
  uint8_t total = sequencer_get_steps_per_bar() * currentSong.numBars;
  if (total > SEQ_MAX_STEPS)
    total = SEQ_MAX_STEPS;
  if (total == 0)
    total = 8;
  return total;
}

void sequencer_set_bpm(uint16_t bpm)
{
  if (bpm < SEQ_MIN_BPM)
    bpm = SEQ_MIN_BPM;
  if (bpm > SEQ_MAX_BPM)
    bpm = SEQ_MAX_BPM;
  currentSong.bpm = bpm;
}

void sequencer_change_bpm(int delta)
{
  int next = (int)currentSong.bpm + delta;
  if (next < SEQ_MIN_BPM)
    next = SEQ_MIN_BPM;
  if (next > SEQ_MAX_BPM)
    next = SEQ_MAX_BPM;
  currentSong.bpm = (uint16_t)next;
}

uint16_t sequencer_get_bpm()
{
  return currentSong.bpm;
}

void sequencer_toggle_time_signature()
{
  currentSong.beatsPerBar = (currentSong.beatsPerBar == 4) ? 3 : 4;
  if (currentPlayStep >= sequencer_get_total_steps())
  {
    currentPlayStep = 0;
  }
}

uint8_t sequencer_get_beats_per_bar()
{
  return currentSong.beatsPerBar;
}

void sequencer_cycle_num_bars()
{
  if (currentSong.numBars == 2)
    currentSong.numBars = 4;
  else if (currentSong.numBars == 4)
    currentSong.numBars = 8;
  else
    currentSong.numBars = 2;

  if (currentPlayStep >= sequencer_get_total_steps())
  {
    currentPlayStep = 0;
  }
}

uint8_t sequencer_get_num_bars()
{
  return currentSong.numBars;
}

bool sequencer_save_current_song(char *outFileName, size_t outSize)
{
  sequencer_stop();
  bool wasI2sEnabled = audio_is_i2s_enabled();
  audio_set_i2s_enabled(false);

  char nomeArquivo[SEQ_FILENAME_LEN];
  snprintf(nomeArquivo, sizeof(nomeArquivo), "song%d.cseq", storage_next_song_index());

  bool ok = storage_save_song(nomeArquivo, currentSong);

  if (outFileName != nullptr && outSize > 0)
  {
    strncpy(outFileName, nomeArquivo, outSize - 1);
    outFileName[outSize - 1] = '\0';
  }

  if (wasI2sEnabled)
  {
    audio_set_i2s_enabled(true);
  }

  return ok;
}

bool sequencer_load_song_by_index(int songIndex)
{
  if (songIndex < 0 || songIndex >= storage_get_song_count())
  {
    return false;
  }

  sequencer_stop();

  char fullPath[SEQ_FILENAME_LEN + 8];
  snprintf(fullPath, sizeof(fullPath), "%s.cseq", storage_get_song_name(songIndex));

  bool wasI2sEnabled = audio_is_i2s_enabled();
  audio_set_i2s_enabled(false);

  bool ok = storage_load_song(fullPath, currentSong);
  if (ok)
  {
    for (int s = 0; s < SEQ_NUM_SYMBOLS; s++)
    {
      slotLoaded[s] = false;
    }
    currentPlayStep = 0;
  }

  if (wasI2sEnabled)
  {
    audio_set_i2s_enabled(true);
  }

  if (ok)
  {
    sequencer_ensure_samples_loaded();
  }

  return ok;
}

const SeqSong &sequencer_get_song()
{
  return currentSong;
}

uint8_t sequencer_staff_index_to_midi(int staffIndex)
{
  if (staffIndex < 0)
    staffIndex = 0;
  if (staffIndex >= SEQ_STAFF_POSITIONS)
    staffIndex = SEQ_STAFF_POSITIONS - 1;
  return STAFF_DIATONIC_MIDI[staffIndex];
}

int sequencer_staff_index_to_y(int staffIndex)
{
  if (staffIndex < 0)
    staffIndex = 0;
  if (staffIndex >= SEQ_STAFF_POSITIONS)
    staffIndex = SEQ_STAFF_POSITIONS - 1;
  return 56 - (staffIndex * 2);
}

void sequencer_midi_to_staff_y(uint8_t midiNote, int &outY, bool &outSharp)
{
  uint8_t clamped = midiNote;
  if (clamped < 41)
    clamped = 41;
  if (clamped > 80)
    clamped = 80;

  for (int i = 0; i < SEQ_STAFF_POSITIONS; i++)
  {
    if (STAFF_DIATONIC_MIDI[i] == clamped)
    {
      outY = 56 - (i * 2);
      outSharp = false;
      return;
    }
  }

  // Se não for natural, corresponde a um sustenido da nota natural logo abaixo
  for (int i = 0; i < SEQ_STAFF_POSITIONS; i++)
  {
    if (STAFF_DIATONIC_MIDI[i] == (uint8_t)(clamped - 1))
    {
      outY = 56 - (i * 2);
      outSharp = true;
      return;
    }
  }

  outY = 34; // Fallback: Dó Central (C4)
  outSharp = false;
}

const char *sequencer_midi_to_note_name(uint8_t midiNote, char *buf, size_t bufSize)
{
  static const char *const SOLFEGE_NAMES[12] = {
      "Dó", "Dó#", "Ré", "Ré#", "Mi", "Fá", "Fá#", "Sol", "Sol#", "Lá", "Lá#", "Si"};

  int octave = (midiNote / 12) - 1;
  const char *name = SOLFEGE_NAMES[midiNote % 12];
  snprintf(buf, bufSize, "%s%d", name, octave);
  return buf;
}

const char *sequencer_midi_to_compact_dual_name(uint8_t midiNote, char *buf, size_t bufSize)
{
  static const char *const SOLFEGE_NAMES[12] = {
      "Dó", "Dó#", "Ré", "Ré#", "Mi", "Fá", "Fá#", "Sol", "Sol#", "Lá", "Lá#", "Si"};
  static const char *const CIFRA_NAMES[12] = {
      "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

  int pc = midiNote % 12;
  int octave = (midiNote / 12) - 1;
  bool isSharp = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);

  if (isSharp)
  {
    snprintf(buf, bufSize, "%s %s", SOLFEGE_NAMES[pc], CIFRA_NAMES[pc]);
  }
  else
  {
    snprintf(buf, bufSize, "%s%d %s", SOLFEGE_NAMES[pc], octave, CIFRA_NAMES[pc]);
  }
  return buf;
}

const char *sequencer_midi_to_full_dual_name(uint8_t midiNote, char *buf, size_t bufSize)
{
  static const char *const SOLFEGE_NAMES[12] = {
      "Dó", "Dó#", "Ré", "Ré#", "Mi", "Fá", "Fá#", "Sol", "Sol#", "Lá", "Lá#", "Si"};
  static const char *const CIFRA_NAMES[12] = {
      "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

  int pc = midiNote % 12;
  int octave = (midiNote / 12) - 1;
  snprintf(buf, bufSize, "%s%d (%s%d)", SOLFEGE_NAMES[pc], octave, CIFRA_NAMES[pc], octave);
  return buf;
}
