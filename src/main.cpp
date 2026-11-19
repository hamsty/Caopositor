#include <Arduino.h>
#include <pico/multicore.h>

#include "config.h"
#include "app_types.h"
#include "audio_engine.h"
#include "pitch_detector.h"
#include "storage.h"
#include "recorder.h"
#include "keyboard.h"
#include "midi_controller.h"
#include "led_matrix.h"
#include "ui.h"

static int instrumentoAtual = -1;
static int mode = 0x01; // testando som

static void onRecordingFinished()
{
  Serial.println("Gravacao finalizada, buffer cheio.");
  setCurrentScreen(Tela::SOM_GRAVADO);
  desenharSomGravado();

  float notaDetectada = pitch_detect_midi_note(audio_get_sample_buffer());
  audio_set_base_midi_note(notaDetectada);

  char nomeArquivo[20];
  snprintf(nomeArquivo, sizeof(nomeArquivo), "recsample%d.wav", storage_next_recorded_index());
  if (storage_save_wav(nomeArquivo, audio_get_sample_buffer(), TAMANHO_BUFFER))
  {
    setInstrumentList(storage_get_sample_list(), storage_get_sample_count());
  }
}

// Processo que roda no núcleo 1 sempre que uma tecla musical for pressionada ou solta
static void onKeyboardNote(uint8_t code, uint8_t note)
{
  switch (mode)
  {
  case 0x01: // Keyboard mode
    audio_handle_key(code, note);
    break;
  case 0x00: // MIDI mode
    midi_handle_key(code, note);
    if (code == 0x90)
    {
      led_matrix_trigger_firework();
    }
    break;
  default:
    break;
  }
}

static void onKeyboardControl(uint8_t key)
{
  switch (key)
  {
  case 40:
    keyboard_octave_down();
    break;
  case 41:
    keyboard_octave_up();
    break;
  case 42:
    audio_volume_down();
    break;
  case 43:
    audio_volume_up();
    break;
  default:
    break;
  }
}

static void resolveUI()
{
  Tela telaAtual = getCurrentScreen();

  switch (telaAtual)
  {
  case Tela::MENU:
    mode = 0x04;
    break;

  case Tela::TECLADO:
    mode = 0x01; // Modo Keyboard
    if (!audio_is_i2s_enabled())
    {
      audio_set_i2s_enabled(true);
    }
    if (getCurrentInstrumentIndex() != instrumentoAtual)
    {
      const char *arquivo = getCurrentInstrumentFile();
      Serial.print("Carregando sample: ");
      Serial.println(arquivo);
      if (storage_load_sample(arquivo, audio_get_sample_buffer(), recorder_get_sample_index_ptr()))
      {
        float notaDetectada = pitch_detect_midi_note(audio_get_sample_buffer());
        audio_set_base_midi_note(notaDetectada);
      }
      instrumentoAtual = getCurrentInstrumentIndex();
    }
    break;

  case Tela::MIDI:
    mode = 0x00; // Modo MIDI
    break;

  case Tela::GRAVAR:
    mode = 0x02;
    break;

  case Tela::GRAVANDO:
    if (!recorder_is_recording())
    {
      recorder_start();
    }
    break;

  case Tela::SOM_GRAVADO:
    break;
  }
}

// Loop do núcleo 1
static void core1_entry()
{
  while (1)
  {
    tight_loop_contents();
    midi_update();
    keyboard_loop();
    ui_update();
    resolveUI();
    led_matrix_update(getCurrentScreen());
  }
}

// Configuração do núcleo principal (Core 0)
void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);

  midi_init();

  if (storage_init())
  {
    setInstrumentList(storage_get_sample_list(), storage_get_sample_count());
  }

  audio_init();

  keyboard_init(onKeyboardNote, onKeyboardControl);

  multicore_launch_core1(core1_entry);
  Serial.println("Core1 started");

  recorder_init(audio_get_sample_buffer(), onRecordingFinished);

  ui_init();

  led_matrix_init();
}

// Loop do núcleo 0
void loop()
{
  audio_loop();
}
