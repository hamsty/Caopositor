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
#include "sequencer.h"
#include "ui.h"

static int instrumentoAtual = -1;
static int mode = 0x01; // 0x00=MIDI, 0x01=Teclado, 0x02=Gravar, 0x03=Compositor, 0x04=Menu

static void onRecordingFinished()
{
  Serial.print("Gravacao finalizada. Amostras capturadas: ");
  Serial.println(recorder_get_sample_index());
  instrumentoAtual = -1; // Força recarga do Modo Teclado após editar/salvar
  ui_open_recording_editor();
}

// Processo que roda no núcleo 1 sempre que uma tecla musical for pressionada ou solta
static void onKeyboardNote(uint8_t code, uint8_t note)
{
  switch (mode)
  {
  case 0x01: // Keyboard mode
    audio_handle_key(code, note, 0);
    if (code == 0x90)
    {
      ui_on_keyboard_note_played(note);
    }
    break;
  case 0x00: // MIDI mode
    midi_handle_key(code, note);
    if (code == 0x90)
    {
      led_matrix_trigger_firework();
    }
    break;
  case 0x03: // Modo Compositor (Sequenciador Mario Paint)
    ui_on_composer_piano_key(code, note);
    break;
  default:
    break;
  }
}

static void onKeyboardControl(uint8_t key)
{
  switch (key)
  {
  case KEY_COMPOSER_REST:
    if (mode == 0x03)
    {
      ui_on_composer_rest_key();
    }
    break;
  case 40:
    audio_volume_up();
    ui_show_volume_popup(audio_get_volume());
    break;
  case 41:
    audio_volume_down();
    ui_show_volume_popup(audio_get_volume());
    break;
  case 42:
    keyboard_octave_down();
    break;
  case 43:
    keyboard_octave_up();
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
    if (getCurrentInstrumentIndex() != instrumentoAtual && storage_get_sample_count() > 0)
    {
      const char *arquivo = getCurrentInstrumentFile();
      if (arquivo != nullptr && arquivo[0] != '\0')
      {
        Serial.print("Carregando sample: ");
        Serial.println(arquivo);

        audio_set_i2s_enabled(false);
        volatile uint32_t loadedLen = 0;
        if (storage_load_sample(arquivo, audio_get_sample_buffer(0), &loadedLen))
        {
          audio_set_slot_length(loadedLen, 0);
          float notaDetectada = pitch_detect_midi_note(audio_get_sample_buffer(0), loadedLen);
          audio_set_base_midi_note(notaDetectada, 0);
          sequencer_mark_slot0_dirty();
        }
        audio_set_i2s_enabled(true);
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
  case Tela::EDITAR_GRAVACAO:
  case Tela::SOM_GRAVADO:
    mode = 0x02;
    break;

  case Tela::COMPOSITOR:
    mode = 0x03;
    instrumentoAtual = -1; // Força recarga do Modo Teclado ao voltar para ele
    if (!audio_is_i2s_enabled())
    {
      audio_set_i2s_enabled(true);
    }
    break;

  case Tela::SEQ_CONFIG:
  case Tela::SEQ_SIMBOLOS:
  case Tela::SEQ_ESCOLHER_SAMPLE:
  case Tela::SEQ_ARQUIVOS:
    mode = 0x04;
    instrumentoAtual = -1;
    break;
  }
}

// Loop do núcleo 1
static void core1_entry()
{
  while (1)
  {
    tight_loop_contents();

    // Atualiza o sequenciador primeiro para disparar notas antes de redesenhar o OLED
    if (sequencer_update())
    {
      if (getCurrentScreen() == Tela::COMPOSITOR)
      {
        ui_redraw_current();
      }
    }

    // Processa conclusão de gravação fora da ISR do timer
    if (recorder_consume_finished())
    {
      onRecordingFinished();
    }

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
  sequencer_init();
  keyboard_init(onKeyboardNote, onKeyboardControl);
  recorder_init(audio_get_record_buffer(), RECORD_MAX_SAMPLES);
  ui_init();
  led_matrix_init();

  multicore_launch_core1(core1_entry);
  Serial.println("Core1 started");
}

// Loop do núcleo 0
void loop()
{
  audio_loop();
}
