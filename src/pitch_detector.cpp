#include "pitch_detector.h"
#include "config.h"
#include <arduinoFFT.h>

static const uint16_t amostrasFFT = FFT_PITCH_SAMPLES;
static const double freqAmostragem = (double)AUDIO_SAMPLE_RATE;

// Arrays para cálculo dos números complexos (usamos float para poupar RAM do RP2040)
static float vReal[FFT_PITCH_SAMPLES];
static float vImag[FFT_PITCH_SAMPLES];

static ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, amostrasFFT, freqAmostragem);

float pitch_detect_midi_note(const int8_t *sampleBuffer)
{
  // Pula as primeiras 2000 amostras (0.12 segundos) para ignorar
  // o ruído de clique do botão ou o silêncio inicial da gravação
  int offset = 2000;

  // 1. Prepara os dados
  for (uint16_t i = 0; i < amostrasFFT; i++)
  {
    vReal[i] = (float)sampleBuffer[i + offset];
    vImag[i] = 0.0; // A parte imaginária começa sempre em zero
  }

  // 2. Aplica a janela de Hamming para suavizar o corte e focar no centro da onda
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);

  // 3. Executa a Transformada (tempo para frequências)
  FFT.compute(FFT_FORWARD);

  // 4. Converte os resultados complexos em amplitudes legíveis
  FFT.complexToMagnitude();

  // 5. Encontra o pico dominante (frequência fundamental)
  // A biblioteca faz uma interpolação matemática para dar precisão decimal
  float frequenciaDetectada = FFT.majorPeak();

  // 6. Filtro de segurança: se detetar ruído de fundo extremo (< 65Hz)
  if (frequenciaDetectada < 65.0 || frequenciaDetectada > 2000.0)
  {
    frequenciaDetectada = 261.63; // Assume Dó Central (C4) como falha segura
  }

  // Converte a Frequência (Hz) para Nota MIDI fracionária
  float notaOriginal = 69.0 + 12.0 * log2(frequenciaDetectada / 440.0);

  Serial.print("FFT Frequencia Dominante: ");
  Serial.print(frequenciaDetectada);
  Serial.print(" Hz | Nota MIDI: ");
  Serial.println(notaOriginal);

  return notaOriginal;
}
