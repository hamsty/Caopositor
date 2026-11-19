#include "led_matrix.h"
#include "config.h"
#include "audio_engine.h"
#include "recorder.h"
#include "sequencer.h"
#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

static Adafruit_NeoPixel matriz = Adafruit_NeoPixel(NUM_LEDS, PINO_MATRIZ, NEO_GRB + NEO_KHZ800);

static unsigned long ultimoUpdateMatriz = 0;

// Arrays para a FFT em tempo real dos LEDs
static float vRealMatriz[AMOSTRAS_MATRIZ];
static float vImagMatriz[AMOSTRAS_MATRIZ];
static ArduinoFFT<float> FFTMatriz = ArduinoFFT<float>(vRealMatriz, vImagMatriz, AMOSTRAS_MATRIZ, (double)AUDIO_SAMPLE_RATE);

static volatile bool fogosAtivos = false;
static unsigned long tempoInicioFogos = 0;
static int centroX = 2;
static int centroY = 2;

static int getLedIndex(int x, int y)
{
  // Invertemos a lógica: o y=0 agora mapeia para a base física da placa
  int linhaFisica = y;

  if (linhaFisica % 2 == 0)
  {
    return (linhaFisica * 5) + (4 - x); // Ziguezague para a esquerda
  }
  else
  {
    return (linhaFisica * 5) + x; // Ziguezague para a direita
  }
}

static void animarGravacao()
{
  if (millis() - ultimoUpdateMatriz < 50)
    return; // Atualiza a 20 FPS
  ultimoUpdateMatriz = millis();

  matriz.clear();

  // 1. Calcula o progresso exato da gravação (de 0.0 a 1.0)
  uint32_t maxSamples = recorder_get_max_samples();
  float progresso = (maxSamples > 0) ? ((float)recorder_get_sample_index() / (float)maxSamples) : 0.0f;

  // O raio máximo num ecrã 5x5 (distância em cruz do centro até a ponta) é 4
  int raioAtual = (int)(progresso * 4.0);

  // Efeito extra: Faz o anel mais externo (que está a crescer) piscar rapidamente
  float pulsoDaBorda = (sin(millis() / 80.0) + 1.0) / 2.0;

  for (int x = 0; x < 5; x++)
  {
    for (int y = 0; y < 5; y++)
    {
      // Calcula a distância geométrica do LED até ao centro (x=2, y=2)
      int distancia = abs(x - 2) + abs(y - 2);

      // Acende os LEDs que já foram alcançados pelo progresso
      if (distancia <= raioAtual)
      {
        int brilho = 3; // Brilho bem fraco e fixo para o "passado" da gravação

        // Se for o LED do momento exato da expansão, aplica o efeito de pulsar (Máximo 10)
        if (distancia == raioAtual)
        {
          brilho = (int)(pulsoDaBorda * 10.0);
          if (brilho < 1)
            brilho = 1; // Não deixa apagar totalmente
        }

        matriz.setPixelColor(getLedIndex(x, y), matriz.Color(brilho, 0, 0));
      }
    }
  }

  matriz.show();
}

static void animarFogos()
{
  if (!fogosAtivos)
    return;

  if (millis() - ultimoUpdateMatriz < 50)
    return;
  ultimoUpdateMatriz = millis();

  unsigned long tempoDecorrido = millis() - tempoInicioFogos;
  matriz.clear();

  if (tempoDecorrido < 100)
  {
    // Fase 1: Semente brilhante no centro (Luz branca/amarelada)
    matriz.setPixelColor(getLedIndex(centroX, centroY), matriz.Color(10, 10, 10));
  }
  else if (tempoDecorrido < 300)
  {
    // Fase 2: Explosão adjacente (Cores aleatórias)
    uint32_t cor = matriz.Color(random(2, 10), random(2, 10), random(2, 10));
    for (int i = -1; i <= 1; i++)
    {
      for (int j = -1; j <= 1; j++)
      {
        if (i == 0 && j == 0)
          continue; // Pula o centro
        int nx = centroX + i;
        int ny = centroY + j;
        if (nx >= 0 && nx < 5 && ny >= 0 && ny < 5)
        {
          matriz.setPixelColor(getLedIndex(nx, ny), cor);
        }
      }
    }
  }
  else if (tempoDecorrido < 500)
  {
    // Fase 3: Dissipação nas pontas (Brilho fraco)
    uint32_t cor = matriz.Color(random(1, 5), random(1, 5), random(1, 5));
    int posicoes[4][2] = {{centroX - 2, centroY}, {centroX + 2, centroY}, {centroX, centroY - 2}, {centroX, centroY + 2}};

    for (int i = 0; i < 4; i++)
    {
      int nx = posicoes[i][0];
      int ny = posicoes[i][1];
      if (nx >= 0 && nx < 5 && ny >= 0 && ny < 5)
      {
        matriz.setPixelColor(getLedIndex(nx, ny), cor);
      }
    }
  }
  else
  {
    // Fase 4: Fim da animação
    fogosAtivos = false;
  }

  matriz.show();
}

static void animarEspectroReal()
{
  if (!audio_is_spectrum_ready())
    return;

  const volatile int *bufferAudioMatriz = audio_get_spectrum_buffer();

  long mediaDC = 0;
  for (int i = 0; i < AMOSTRAS_MATRIZ; i++)
  {
    mediaDC += bufferAudioMatriz[i];
  }
  mediaDC /= AMOSTRAS_MATRIZ;

  // Carrega o áudio removendo o DC offset
  for (int i = 0; i < AMOSTRAS_MATRIZ; i++)
  {
    vRealMatriz[i] = (float)(bufferAudioMatriz[i] - mediaDC);
    vImagMatriz[i] = 0.0;
  }

  audio_release_spectrum_buffer();

  FFTMatriz.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFTMatriz.compute(FFT_FORWARD);
  FFTMatriz.complexToMagnitude();

  float picos[5] = {0, 0, 0, 0, 0};

  for (int i = 1; i < (AMOSTRAS_MATRIZ / 2); i++)
  {
    int col = 0;
    if (i <= 3)
      col = 0; // Graves
    else if (i <= 8)
      col = 1; // Médios-Graves
    else if (i <= 18)
      col = 2; // Médios
    else if (i <= 40)
      col = 3; // Médios-Agudos
    else
      col = 4; // Agudos

    // Guarda apenas o valor mais alto daquela coluna
    if (vRealMatriz[i] > picos[col])
    {
      picos[col] = vRealMatriz[i];
    }
  }

  matriz.clear();

  float limiteMinimo = 500.0;    // Valores abaixo disso ficam com 0 LEDs
  float limiteMaximo = 300000.0; // Valores acima disso acendem os 5 LEDs

  float logMin = log10(limiteMinimo);
  float logMax = log10(limiteMaximo);

  for (int col = 0; col < 5; col++)
  {
    int intensidade = 0;

    if (picos[col] > limiteMinimo)
    {
      float meuLog = log10(picos[col]);

      if (meuLog > logMax)
        meuLog = logMax;

      // Regra de três para espalhar o volume certinho de 1 a 5 LEDs
      intensidade = (int)(((meuLog - logMin) / (logMax - logMin)) * 5.0) + 1;
    }

    if (intensidade > 5)
      intensidade = 5;
    if (intensidade < 0)
      intensidade = 0;

    for (int y = 0; y < intensidade; y++)
    {
      int indice = getLedIndex(col, y);

      uint32_t cor;
      if (y < 2)
        cor = matriz.Color(0, 5, 0); // Verde
      else if (y < 4)
        cor = matriz.Color(5, 5, 0); // Amarelo
      else
        cor = matriz.Color(5, 0, 0); // Vermelho

      matriz.setPixelColor(indice, cor);
    }
  }

  matriz.show();
}

void led_matrix_init()
{
  matriz.begin();
  matriz.show();
}

void led_matrix_trigger_firework()
{
  fogosAtivos = true;
  tempoInicioFogos = millis();
  centroX = random(1, 4); // Escolhe um X aleatório (evita as bordas extremas)
  centroY = random(1, 4);
}

void led_matrix_update(Tela telaAtual)
{
  if (telaAtual == Tela::TECLADO || (telaAtual == Tela::COMPOSITOR && sequencer_is_playing()))
  {
    animarEspectroReal();
  }
  else if (telaAtual == Tela::GRAVANDO)
  {
    animarGravacao();
  }
  else if (telaAtual == Tela::MIDI)
  {
    if (fogosAtivos)
    {
      animarFogos();
    }
    else
    {
      if (millis() - ultimoUpdateMatriz > 50)
      {
        matriz.clear();
        matriz.show();
        ultimoUpdateMatriz = millis();
      }
    }
  }
  else
  {
    if (millis() - ultimoUpdateMatriz > 200)
    {
      matriz.clear();
      matriz.show();
      ultimoUpdateMatriz = millis();
    }
  }
}
