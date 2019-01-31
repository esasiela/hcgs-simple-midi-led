
#include <MIDI.h>
#include <Adafruit_NeoPixel.h>

#include <HC_BouncyButton.h>

/* SoftwareSerial is way slower and loses messages pretty easily.
 * HardwareSerial performance is very nice, but you lose the SerialMonitor for debugging,
 * and have to keep switching for programming (unless using the ICSP header)
 */

// useful for MIDI debugging
//#define __SERIAL_DEBUG__ 1

// this one outputs debugging for color manipulation, not for MIDI debugging
// #define __SERIAL_DEBUG_COLOR__ 1

//#define __MIDI_SOFTWARE_SERIAL__ 1





#ifdef __MIDI_SOFTWARE_SERIAL__

#include <SoftwareSerial.h>
// args are Rx, Tx
SoftwareSerial midiSerial(9, 10);
MIDI_CREATE_INSTANCE(SoftwareSerial, midiSerial, midiCtl);

#else

// probably don't want to also define __SERIAL_DEBUG__, they conflict
#ifndef __SERIAL_DEBUG__
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiCtl);
#endif

#endif


#define PIN_LED_R 7
#define PIN_LED_G 6
#define PIN_LED_B 8

#define PIN_BTN_R 3
#define PIN_BTN_G 4
#define PIN_BTN_B 5

#define PIN_NEO A3

// undefine MIDI_CHANNEL to listen to all channels
//#define MIDI_CHANNEL 2


#define MIDI_NOTE_R 60
#define MIDI_NOTE_G 64
#define MIDI_NOTE_B 67

#define MIDI_NOTE_ON 144
#define MIDI_NOTE_OFF 128


byte midiStatusByte;
byte midiDataByte1;
byte midiDataByte2;
byte midiChannel;
byte midiCommand;
byte midiNote;
byte midiVelocity;

// even though we only need uint8_t, using int because increment will wrap out of bounds
#define NUM_COLORS 3
int colorValues[] = {0, 0, 0};
int8_t colorDirections[] = {-1, -1, -1};
int colorNotes[] = {MIDI_NOTE_R, MIDI_NOTE_G, MIDI_NOTE_B};
byte colorLedPins[] = {PIN_LED_R, PIN_LED_G, PIN_LED_B};

BouncyButton colorButtons[] = {
  BouncyButton(PIN_BTN_R),
  BouncyButton(PIN_BTN_G),
  BouncyButton(PIN_BTN_B)
};




// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = (OPTIONAL) pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel neo = Adafruit_NeoPixel(1, PIN_NEO);

unsigned long lastUpdateMillis;
#define UPDATE_INTERVAL_MILLIS 100

#define UPDATE_COLOR_INCREMENT 3

void setup() {

#ifdef __SERIAL_DEBUG__
  Serial.begin(9600);
  delay(100);
#endif

  neo.begin();
  // initializes all pixels OFF
  neo.show();

  midiCtl.setHandleNoteOn(noteOn);
  midiCtl.setHandleNoteOff(noteOff);
  midiCtl.begin(MIDI_CHANNEL_OMNI);


  for (byte x=0; x<NUM_COLORS; x++) {
    pinMode(colorLedPins[x], OUTPUT);
    digitalWrite(colorLedPins[x], HIGH);

    pinMode(colorButtons[x].getPin(), INPUT_PULLUP);
    colorButtons[x].init();
  }
}

void loop() {
  midiCtl.read();

  // check for debounced state changes in the pushbuttons
  for (byte x=0; x<NUM_COLORS; x++) {
    if (colorButtons[x].update()) {
      if (!colorButtons[x].getState()) {
        // pressed (gnd)
        colorEnable(x);
      } else {
        // released (pullup reads high)
        colorDisable(x);
      }
    }
  }

  // periodically change the color intensity values for any active colors
  if ((millis()-lastUpdateMillis)>UPDATE_INTERVAL_MILLIS) {
    lastUpdateMillis=millis();

    boolean doUpdate=false;
    for (byte x=0; x<NUM_COLORS; x++) {
      if (colorValues[x]>0) {
        doUpdate=true;
        
#ifdef __SERIAL_DEBUG_COLOR__
        Serial.print(F("manipColor "));
        Serial.print(x);
        Serial.print(F(" - "));
        Serial.print(colorValues[x]);
#endif

        // move the color value in the direction that this color is moving (brighter or dimmer)
        colorValues[x] += colorDirections[x] * UPDATE_COLOR_INCREMENT;      

#ifdef __SERIAL_DEBUG_COLOR__
        Serial.print(F(" -> "));
        Serial.println(colorValues[x]);
#endif

        // Wrap the values when you overflow above or below the 1-255 range for an active color
        if (colorValues[x]>255) {
          colorValues[x] = 255;
          colorDirections[x] = -1;
        } else if (colorValues[x]<=0) {
          colorValues[x] = 1;
          colorDirections[x] = 1;
        }
      }
    }
    if (doUpdate) {
      updateDisplay();
    }
  }
}

void noteOn(byte channel, byte note, byte velocity) {
#ifdef __SERIAL_DEBUG__

  Serial.print(F("NOTE ON  channel: "));
  Serial.print(channel);
  Serial.print(F(" note: "));
  Serial.print(note);
  Serial.print(F(" vel: "));
  Serial.println(velocity);

#endif


#ifdef MIDI_CHANNEL
  if (channel == MIDI_CHANNEL) {
#endif   
  
  for (int x=0; x<NUM_COLORS; x++) {
    if (note == colorNotes[x] && velocity > 0) {
      colorEnable(x);
    }
  }

#ifdef MIDI_CHANNEL
  }
#endif
  
}

void noteOff(byte channel, byte note, byte velocity) {
#ifdef __SERIAL_DEBUG__
  Serial.print(F("NOTE OFF channel: "));
  Serial.print(channel);
  Serial.print(F(" note: "));
  Serial.print(note);
  Serial.print(F(" vel: "));
  Serial.println(velocity);
#endif

#ifdef MIDI_CHANNEL
  if (channel == MIDI_CHANNEL) {
#endif

  for (int x=0; x<NUM_COLORS; x++) {
    if (note == colorNotes[x]) {
      colorDisable(x);
    }
  }

#ifdef MIDI_CHANNEL
  }
#endif
} 

void updateDisplay() {
  // (pixelIndex, green, red, blue)
  neo.setPixelColor(0, colorValues[1], colorValues[0], colorValues[2]);
  neo.show();
}

void colorEnable(byte cIdx) {
#ifdef __SERIAL_DEBUG_COLOR__
  Serial.print(F("colorEnable "));
  Serial.println(cIdx);
#endif
  
  digitalWrite(colorLedPins[cIdx], LOW);
  colorValues[cIdx] = 255;
  updateDisplay();
}

void colorDisable(byte cIdx) {
#ifdef __SERIAL_DEBUG_COLOR__
  Serial.print(F("colorDisable "));
  Serial.println(cIdx);
#endif
  
  digitalWrite(colorLedPins[cIdx], HIGH);
  colorValues[cIdx] = 0;
  updateDisplay();
}
