#include "pitch_detector.h"
#include "config.h"
#include <arduinoFFT.h>
#include <new>

float pitch_detect_midi_note(const int8_t *sampleBuffer, uint32_t sampleLength)
{
  if (sampleBuffer == nullptr || sampleLength == 0)
  {
    return 60.0f;
  }

  // Alocação temporária para não ocupar RAM permanentemente
  float *vReal = new (std::nothrow) float[FFT_PITCH_SAMPLES];
  float *vImag = new (std::nothrow) float[FFT_PITCH_SAMPLES];

  if (vReal == nullptr || vImag == nullptr)
  {
    delete[] vReal;
    delete[] vImag;
    Serial.println("FFT: memoria insuficiente, assumindo C4.");
    return 60.0f;
  }

  // Pula as primeiras 2000 amostras (~0.12s) quando o sample é longo o suficiente
  uint32_t offset = (sampleLength >= (2000 + FFT_PITCH_SAMPLES)) ? 2000 : 0;

  // Calcula DC offset para melhorar precisão em gravações de microfone
  float mediaDC = 0.0f;
  uint32_t validCount = 0;
  for (uint16_t i = 0; i < FFT_PITCH_SAMPLES; i++)
  {
    uint32_t idx = offset + i;
    if (idx < sampleLength)
    {
      mediaDC += (float)sampleBuffer[idx];
      validCount++;
    }
  }
  if (validCount > 0)
  {
    mediaDC /= (float)validCount;
  }

  for (uint16_t i = 0; i < FFT_PITCH_SAMPLES; i++)
  {
    uint32_t idx = offset + i;
    vReal[i] = (idx < sampleLength) ? ((float)sampleBuffer[idx] - mediaDC) : 0.0f;
    vImag[i] = 0.0f;
  }

  ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, FFT_PITCH_SAMPLES, (double)AUDIO_SAMPLE_RATE);

  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  float frequenciaDetectada = FFT.majorPeak();

  delete[] vReal;
  delete[] vImag;

  // Filtro de segurança: se detetar ruído de fundo extremo (< 65Hz ou > 2000Hz)
  if (frequenciaDetectada < 65.0f || frequenciaDetectada > 2000.0f)
  {
    frequenciaDetectada = 261.63f; // Assume Dó Central (C4) como falha segura
  }

  // Converte a Frequência (Hz) para Nota MIDI fracionária
  float notaOriginal = 69.0f + 12.0f * log2f(frequenciaDetectada / 440.0f);

  Serial.print("FFT Frequencia Dominante: ");
  Serial.print(frequenciaDetectada);
  Serial.print(" Hz | Nota MIDI: ");
  Serial.println(notaOriginal);

  return notaOriginal;
}
