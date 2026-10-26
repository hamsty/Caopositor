#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Control_Surface.h>
#include <Wire.h>

Adafruit_MCP23X17 mcp;

USBMIDI_Interface  midi; 
// USBDebugMIDI_Interface midi(115200);

const Channel channel = Channel_1;

int key_old_state[56];

void setup()
{
  Serial.begin(115200);

  delay(500);

  Serial.println("Starting Keyboard Test");

  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.begin();

  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(11, OUTPUT);


  if (!mcp.begin_I2C(0x20, &Wire1))
  {
    Serial.println("Error.");
    
    while (1){
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

  for(int i = 0; i < 48;i++){
    key_old_state[i] = false;
  }

  midi.begin();
}



void loop()
{
  for (int i = 0; i < 7; i++)
  {
    mcp.digitalWrite(i+8, LOW);
    delayMicroseconds(2);
    for (int j = 0; j < 8; j++)
    {
      uint8_t key = i * 8 + (7-j);
      int key_state = mcp.digitalRead(j);
      if (key_state != key_old_state[key])
      {
        key_old_state[key] = key_state;
        if(key_old_state[key] == LOW){
          midi.sendNoteOn({53+key, channel}, 100);
          // Serial.printf("key %d pressed, %d %d \n", key, i,j);
        }else{
          midi.sendNoteOff({53+key, channel}, 100);
        }
      }
    }
    mcp.digitalWrite(i+8, HIGH);
    delayMicroseconds(2);
  }
  midi.update(); 
}
