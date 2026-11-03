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

#include <Mozzi.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <mozzi_midi.h>
#include <ADSR.h>

using namespace std;

Adafruit_MCP23X17 mcp;

// USBMIDI_Interface midi;
USBDebugMIDI_Interface midi(115200);

Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> aSin(SIN2048_DATA);

// // envelope generator
ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelope;

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
    aSin.setFreq_Q16n16(Q16n16_mtof(Q8n0_to_Q16n16(note))); // accurate frequency
    envelope.noteOn();
    break;
  case 0x80:
    envelope.noteOff();
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
    case 0x00: // midi mode
      midi_handle(code, note);
      break;
    default:
      break;
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
          handle_keyboard_event(0x90, 53+key);
          Serial.printf("key %d pressed, %d %d \n", key, i, j);
        }
        else
        {
          handle_keyboard_event(0x80, 53+key);
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
  }
}

void updateControl(){
  envelope.update();
}


AudioOutput updateAudio(){
  return MonoOutput::from16Bit(envelope.next() * aSin.next());
}


// configuração do núcleo principal
void setup()
{

  Serial.begin(115200);

  while(!Serial){
    delay(20);
  }

  midi.begin();

  Serial.println("MIDI Started");

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

  envelope.setADLevels(255,64);
  envelope.setTimes(50,200,10000,200); 

  aSin.setFreq(440); // default frequency
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
