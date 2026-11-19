#include "keyboard.h"
#include "config.h"
#include <Adafruit_MCP23X17.h>
#include <Wire.h>

static Adafruit_MCP23X17 mcp;
static int key_old_state[KEYBOARD_TOTAL_KEYS];
static uint8_t offset = KEYBOARD_DEFAULT_OFFSET;

static KeyNoteCallback noteCallback = nullptr;
static KeyControlCallback controlCallback = nullptr;

void keyboard_init(KeyNoteCallback onNote, KeyControlCallback onControl)
{
  noteCallback = onNote;
  controlCallback = onControl;

  Wire1.setSDA(KEYBOARD_SDA_PIN);
  Wire1.setSCL(KEYBOARD_SCL_PIN);
  Wire1.begin();

  pinMode(LED_STATUS_B_PIN, OUTPUT);
  pinMode(LED_STATUS_R_PIN, OUTPUT);
  pinMode(LED_STATUS_G_PIN, OUTPUT);

  if (!mcp.begin_I2C(KEYBOARD_I2C_ADDR, &Wire1))
  {
    Serial.println("Error.");

    while (1)
    {
      digitalWrite(LED_STATUS_R_PIN, HIGH);
      delay(200);
      digitalWrite(LED_STATUS_R_PIN, LOW);
      delay(200);
    }
  }

  digitalWrite(LED_STATUS_B_PIN, HIGH);

  // Pré-define o latch de saída das linhas (Port B: pinos 8..14) como LOW
  // e mantém todas em alta impedância (INPUT) para evitar ghosting/conflito elétrico
  mcp.writeGPIOB(0x00);
  for (int row = 8; row <= 14; row++)
  {
    mcp.pinMode(row, INPUT);
  }

  // Colunas (Port A: pinos 0..7) com pull-up interno
  for (int col = 0; col <= 7; col++)
  {
    mcp.pinMode(col, INPUT_PULLUP);
  }

  for (int i = 0; i < KEYBOARD_TOTAL_KEYS; i++)
  {
    key_old_state[i] = HIGH;
  }

  Serial.println("Keyboard configuration done!");
}

void keyboard_octave_up()
{
  if (offset < (255 - KEYBOARD_NOTE_KEYS))
  {
    offset += 12;
    Serial.print("Octave Up! Offset: ");
    Serial.println(offset);
  }
}

void keyboard_octave_down()
{
  if (offset > KEYBOARD_NOTE_KEYS)
  {
    offset -= 12;
    Serial.print("Octave Down! Offset: ");
    Serial.println(offset);
  }
}

// Varredura anti-ghosting: apenas a linha ativa vai para OUTPUT LOW; as demais ficam em alta impedância (INPUT)
void keyboard_loop()
{
  for (int i = 0; i < 7; i++)
  {
    uint8_t rowPin = (uint8_t)(i + 8);

    // Ativa somente a linha atual em nível baixo
    mcp.pinMode(rowPin, OUTPUT);
    mcp.digitalWrite(rowPin, LOW);
    delayMicroseconds(5);

    // Lê todas as 8 colunas (Port A) em uma única transação I2C
    uint8_t cols = mcp.readGPIOA();

    // Retorna imediatamente a linha para alta impedância (INPUT)
    mcp.pinMode(rowPin, INPUT);

    for (int j = 0; j < 8; j++)
    {
      uint8_t key = (uint8_t)(i * 8 + (7 - j));
      int key_state = ((cols >> j) & 0x01) ? HIGH : LOW;

      if (key_state != key_old_state[key])
      {
        key_old_state[key] = key_state;
        if (key_old_state[key] == LOW)
        {
          if (key < KEYBOARD_NOTE_KEYS)
          {
            if (noteCallback != nullptr)
            {
              noteCallback(0x90, offset + key);
            }
            Serial.printf("key %d pressed, %d %d \n", key, i, j);
          }
          else
          {
            if (controlCallback != nullptr)
            {
              controlCallback(key);
            }
          }
        }
        else
        {
          if (key < KEYBOARD_NOTE_KEYS)
          {
            if (noteCallback != nullptr)
            {
              noteCallback(0x80, offset + key);
            }
          }
        }
      }
    }
  }
}
