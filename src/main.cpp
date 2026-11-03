#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Control_Surface.h>
#include <Wire.h>
#include <pico/multicore.h>
#include <SD.h>
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_CONTROL_RATE 128 // Hz, powers of 2 are most reliable

#define MOZZI_I2S_PIN_BCK 14
#define MOZZI_I2S_PIN_DATA 20
#define MOZZI_AUDIO_RATE 16384

#include <Mozzi.h>
#include <Sample.h>
#include <mozzi_midi.h>
#include <ADSR.h>

using namespace std;

#define TAMANHO_BUFFER 32768

int8_t loadedSample[TAMANHO_BUFFER];

volatile bool gravando = false;
volatile uint32_t loadedSampleMaxIndex = 0;

const float FREQ_BASE = (float)AUDIO_RATE / (float)TAMANHO_BUFFER;

Adafruit_MCP23X17 mcp;

// USBMIDI_Interface midi;
USBDebugMIDI_Interface midi(115200);

Sample<TAMANHO_BUFFER, AUDIO_RATE> vozes[10]{
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample),
    Sample<TAMANHO_BUFFER, AUDIO_RATE>(loadedSample)};

// // envelope generator
ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelopes[10]{
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

bool vozes_ativas[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

uint8_t vozes_nota[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const Channel channel = Channel_1;

int key_old_state[56];

int mode = 0x01; // testando som

// inicia a comunicação I2C e configura pinos do teclado
void keyboard_config()
{
  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.begin();

  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(11, OUTPUT);

  if (!mcp.begin_I2C(0x20, &Wire1))
  {
    Serial.println("Error.");

    while (1)
    {
      digitalWrite(13, HIGH);
      delay(200);
      digitalWrite(13, LOW);
      delay(200);
    }
  }

  digitalWrite(12, HIGH);

  mcp.pinMode(8, OUTPUT);
  mcp.pinMode(9, OUTPUT);
  mcp.pinMode(10, OUTPUT);
  mcp.pinMode(11, OUTPUT);
  mcp.pinMode(12, OUTPUT);
  mcp.pinMode(13, OUTPUT);
  mcp.pinMode(14, OUTPUT);

  mcp.pinMode(7, INPUT_PULLUP);
  mcp.pinMode(6, INPUT_PULLUP);
  mcp.pinMode(5, INPUT_PULLUP);
  mcp.pinMode(4, INPUT_PULLUP);
  mcp.pinMode(3, INPUT_PULLUP);
  mcp.pinMode(2, INPUT_PULLUP);
  mcp.pinMode(1, INPUT_PULLUP);
  mcp.pinMode(0, INPUT_PULLUP);

  for (int i = 0; i < 48; i++)
  {
    key_old_state[i] = HIGH;
  }
}

void midi_handle(uint8_t code, uint8_t note)
{
  switch (code)
  {
  case 0x90:
    midi.sendNoteOn({note, channel}, 100);
    break;
  case 0x80:
    midi.sendNoteOff({note, channel}, 100);
    break;
  default:
    break;
  }
}

void keysound_handle(uint8_t code, uint8_t note)
{
  switch (code)
  {
  case 0x90:
    for (int i = 0; i < 10; i++)
    {
      if (!vozes_ativas[i])
      {
        // Calcula o deslocamento de tom (pitch). Assumimos 60 (Dó4) como a nota do som original gravado.
        float multiplicador = pow(2.0, (note - 60.0) / 12.0);
        vozes[i].setFreq(FREQ_BASE * multiplicador);

        vozes[i].start(); // Reinicia o ponteiro de leitura do sample para o início
        envelopes[i].noteOn();
        vozes_nota[i] = note;
        vozes_ativas[i] = true;
        break;
      }
      if (i == 9)
      {
        Serial.println("Todas as vozes estão ocupadas!");
        return;
      }
    }

    break;
  case 0x80:
    for (int i = 0; i < 10; i++)
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

// processo que roda no núcleo 1 sempre que uma tecla for pressionada ou solta.
void handle_keyboard_event(uint8_t code, uint8_t note)
{
  switch (mode)
  {
  case 0x01: // Keyboard mode
    keysound_handle(code, note);
    break;
  case 0x00: // midi mode
    midi_handle(code, note);
    break;
  default:
    break;
  }
}

uint32_t encontrarInicioDados(File &arquivo)
{
  arquivo.seek(0);
  char buffer[4];

  // Procura pela sequência de caracteres "data"
  while (arquivo.available() >= 4)
  {
    buffer[0] = arquivo.read();

    if (buffer[0] == 'd')
    {
      buffer[1] = arquivo.read();
      buffer[2] = arquivo.read();
      buffer[3] = arquivo.read();

      if (buffer[1] == 'a' && buffer[2] == 't' && buffer[3] == 'a')
      {
        // Pula os 4 bytes seguintes (que guardam o tamanho do bloco de áudio)
        arquivo.seek(arquivo.position() + 4);
        return arquivo.position(); // Retorna o tamanho exato do cabeçalho
      }

      // Se não for "data", volta 3 passos atrás para não saltar letras por engano
      arquivo.seek(arquivo.position() - 3);
    }
  }
  return 44; // Retorna o padrão 44 se o bloco não for encontrado
}

void carregarSampleDoSD(const char *nomeArquivo)
{
  File arquivoWav = SD.open(nomeArquivo);

  if (arquivoWav)
  {
    uint32_t tamanhoCabecalho = encontrarInicioDados(arquivoWav);
    Serial.print("Tamanho do cabecalho detetado: ");
    Serial.println(tamanhoCabecalho);
    arquivoWav.seek(tamanhoCabecalho); // Pula o cabeçalho padrão de 44 bytes do formato WAV

    loadedSampleMaxIndex = 0; // Reinicia o índice do buffer global

    // Lê os dados brutos até encher o buffer ou o arquivo acabar
    while (arquivoWav.available() && loadedSampleMaxIndex < TAMANHO_BUFFER)
    {
      // Lê o byte (0 a 255) e subtrai 128 para centralizar a onda (-128 a 127)
      loadedSample[loadedSampleMaxIndex] = arquivoWav.read() - 128;
      loadedSampleMaxIndex++;
    }

    arquivoWav.close();
    Serial.print("Sample carregado com sucesso: ");
    Serial.println(nomeArquivo);
  }
  else
  {
    Serial.println("Erro ao abrir o arquivo no SD!");
  }
}

// Escaneia matriz do teclado para saber qual tecla foi pressionada e envia para o core 1 para processamento do audio e midi.
void keyboard_loop()
{
  for (int i = 0; i < 7; i++)
  {
    mcp.digitalWrite(i + 8, LOW);
    delayMicroseconds(2);
    for (int j = 0; j < 8; j++)
    {
      uint8_t key = i * 8 + (7 - j);
      int key_state = mcp.digitalRead(j);
      if (key_state != key_old_state[key])
      {
        key_old_state[key] = key_state;
        if (key_old_state[key] == LOW)
        {
          handle_keyboard_event(0x90, 53 + key);
          Serial.printf("key %d pressed, %d %d \n", key, i, j);
        }
        else
        {
          handle_keyboard_event(0x80, 53 + key);
        }
      }
    }
    mcp.digitalWrite(i + 8, HIGH);
    delayMicroseconds(2);
  }
}

void test_spi()
{
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  for (int i = 0; i < 10; i++)
  {
    SPI.transfer(0xFF);
  }

  digitalWrite(SS, LOW); // Abaixa o CS para selecionar o cartão
  delay(1);

  char cmd0[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
  for (int i = 0; i < 6; i++)
  {
    SPI.transfer(cmd0[i]);
  }

  // 3. Aguarda a resposta do cartão
  char resposta = 0xFF;
  int tentativas = 0;

  // O cartão pode demorar alguns ciclos para responder, então ficamos ouvindo
  while (resposta == 0xFF && tentativas < 100)
  {
    resposta = SPI.transfer(0xFF);
    tentativas++;
  }

  digitalWrite(SS, HIGH); // Libera o cartão
  SPI.endTransaction();

  // 4. Analisa o resultado
  Serial.print("Resposta recebida: 0x");
  if (resposta < 16)
    Serial.print("0");
  Serial.println(resposta, HEX);

  if (resposta == 0x01)
  {
    Serial.println("\n-> SUCESSO ABSOLUTO!");
    Serial.println("O cartao SD esta se comunicando perfeitamente pelo SPI.");
  }
  else if (resposta == 0xFF)
  {
    Serial.println("\n-> FALHA: Nenhuma resposta (0xFF).");
    Serial.println("O pino MISO (RX) nao esta recebendo nada. Verifique MISO, MOSI, SCK e VCC.");
  }
  else
  {
    Serial.println("\n-> FALHA: Resposta irreconhecivel.");
    Serial.println("Os fios podem estar trocados ou ha muito ruido na linha.");
  }
}

// Loop do núcleo 1
void core1_entry()
{

  while (1)
  {
    tight_loop_contents();
    midi.update();
    keyboard_loop();
    // Olha se o botão A foi pressionado e muda o sample carregado no buffer global
    if (digitalRead(5) == LOW)
    {
      carregarSampleDoSD("yogurt.wav");
      delay(500);
    }
    // olha se o botão B foi pressionado e muda o sample carregado no buffer global
    if (digitalRead(6) == LOW)
    {
      carregarSampleDoSD("grampiano.wav");
      delay(500);
    }
  }
}

void updateControl()
{
  for (int i = 0; i < 10; i++)
  {
    envelopes[i].update();
  }
}

AudioOutput updateAudio()
{
  int32_t mix = 0;
  for (int i = 0; i < 10; i++)
  {
    if (vozes_ativas[i])
    {
      mix += (envelopes[i].next() * vozes[i].next());
    }
  }

  return MonoOutput::from16Bit(mix / 4);
}

// configuração do núcleo principal
void setup()
{

  Serial.begin(115200);

  while (!Serial)
  {
    delay(20);
  }

  midi.begin();

  Serial.println("MIDI Started");

  pinMode(5, INPUT_PULLUP); // botao A
  pinMode(6, INPUT_PULLUP); // botao B

  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);

  SPI.setMISO(MISO);
  SPI.setMOSI(MOSI);
  SPI.setSCK(SCK);

  SPI.begin();

  if (!SD.begin(SS, SPI))
  {
    Serial.println("SD card initialization failed!");
  }
  else

  {

    Serial.println("SD card initialized.");
  }

  for (int i = 0; i < 10; i++)
  {
    envelopes[i].setADLevels(255, 64);
    envelopes[i].setTimes(50, 200, 10000, 200);
  }

  startMozzi();

  Serial.println("Mozzi Started");

  keyboard_config();

  Serial.println("Keyboard configuration done!");

  multicore_launch_core1(core1_entry);

  Serial.println("Core1 started");
}

// Loop do núcleo 0
void loop()
{
  audioHook();
}
