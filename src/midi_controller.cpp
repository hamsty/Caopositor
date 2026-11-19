#include "midi_controller.h"
#include <Control_Surface.h>

static USBMIDI_Interface midi;
// static USBDebugMIDI_Interface midi(115200);
static const Channel channel = Channel_1;

void midi_init()
{
  midi.begin();
  Serial.println("MIDI Started");
}

void midi_update()
{
  midi.update();
}

void midi_handle_key(uint8_t code, uint8_t note)
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
