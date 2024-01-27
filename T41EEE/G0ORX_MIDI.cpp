#ifndef BEENHERE
#include "SDT.h"
#endif

#ifdef G0ORX_MIDI
#include "USBHost_t36.h"

USBHost myusb;
MIDIDevice midi(myusb);



void OnNoteOn(byte channel, byte note, byte velocity) {
	Serial.print("Note On, ch=");
	Serial.print(channel);
	Serial.print(", note=");
	Serial.print(note);
	Serial.print(", velocity=");
	Serial.print(velocity);
	Serial.println();
}

void OnNoteOff(byte channel, byte note, byte velocity) {
	Serial.print("Note Off, ch=");
	Serial.print(channel);
	Serial.print(", note=");
	Serial.print(note);
	//Serial.print(", velocity=");
	//Serial.print(velocity);
	Serial.println();
}

void OnControlChange(byte channel, byte control, byte value) {
	Serial.print("Control Change, ch=");
	Serial.print(channel);
	Serial.print(", control=");
	Serial.print(control);
	Serial.print(", value=");
	Serial.print(value);
	Serial.println();
}

void MIDI_setup() {
  myusb.begin();
  midi.setHandleNoteOff(OnNoteOff);
	midi.setHandleNoteOn(OnNoteOn);
	midi.setHandleControlChange(OnControlChange);
}

void MIDI_loop() {
  myusb.Task();
	midi.read();
}
#endif
