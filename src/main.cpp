#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Control_Surface.h>
#include <Wire.h>
#include "pico/multicore.h"

Adafruit_MCP23X17 mcp;

USBMIDI_Interface midi;
// USBDebugMIDI_Interface midi(115200);

const Channel channel = Channel_1;

int key_old_state[56];

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
    key_old_state[i] = false;
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
          // midi.sendNoteOn({53+key, channel}, 100);
          multicore_fifo_push_blocking(53 + key + (0x90 << 8));
          // Serial.printf("key %d pressed, %d %d \n", key, i,j);
        }
        else
        {
          multicore_fifo_push_blocking(53 + key + (0x80 << 8));
          // midi.sendNoteOff({53+key, channel}, 100);
        }
      }
    }
    mcp.digitalWrite(i + 8, HIGH);
    delayMicroseconds(2);
  }
}

//processo que roda no núcleo 1 sempre que uma tecla for pressionada ou solta.
void core1_interrupt_handler()
{
  while (multicore_fifo_rvalid())
  {
    uint32_t raw = multicore_fifo_pop_blocking();
    int note = raw & 0xFF;
    int code = (raw >> 8) & 0xFF;
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

  multicore_fifo_clear_irq(); // Clear interrupt
}

//Loop do núcleo 1
void core1_entry()
{
  midi.begin();

  multicore_fifo_clear_irq();

  irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);

  irq_set_enabled(SIO_IRQ_PROC1, true);

  while (1)
  {
    tight_loop_contents();
    midi.update();
  }
}

//configuração do núcleo principal
void setup()
{
  multicore_launch_core1(core1_entry);

  Serial.begin(115200);

  delay(500);

  keyboard_config();
}

//Loop do núcleo 0
void loop()
{
  keyboard_loop();
}
