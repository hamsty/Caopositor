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

// Escaneia matriz do teclado para saber qual tecla foi pressionada
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
    mcp.digitalWrite(i + 8, HIGH);
    delayMicroseconds(2);
  }
}
