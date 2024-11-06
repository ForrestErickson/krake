/* robot_api.cpp
   the "robot" (low-level hardware) api

  Copyright (C) 2022 Robert Read

  This program includes free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  See the GNU Affero General Public License for more details.
  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/

#include <arduino.h>
#include "robot_api.h"
#include "alarm_api.h"
#include "gpad_utility.h"
#include<SPI.h>

#include <DailyStruggleButton.h>
DailyStruggleButton muteButton;

extern const char *AlarmNames[];
extern AlarmLevel currentLevel;
extern bool currentlyMuted;
extern char AlarmMessageBuffer[81];

//For LCD
#include <LiquidCrystal_I2C.h>
#ifdef HMWK
LiquidCrystal_I2C lcd(0x27, 20, 4); // set for default LCD address of 0x27 for a 20 chars and 4 line as in HW
#elif
LiquidCrystal_I2C lcd(0x38, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display in Wokwi, and 0x38 for the physical GPAD board
#endif

//Setup for buzzer.
//const int BUZZER_TEST_FREQ = 130; // One below middle C3. About 67 db, 3" x 4.875" 8 Ohm speakers no cabinet at 1 Meter.
//const int BUZZER_TEST_FREQ = 260; // Middle C4. About ?? db, 3" x 4.875" 8 Ohm speakers no cabinet at 1 Meter.
//const int BUZZER_TEST_FREQ = 1000; //About 76 db, 3" x 4.875" 8 Ohm speakers no cabinet at 1 Meter.
const int BUZZER_TEST_FREQ = 4000; // Buzzers, 3 V 4kHz 60dB @ 3V, 10 cm.  The specified frequencey for the Version 1 buzzer.

const int BUZZER_LVL_FREQ_HZ[] = {0, 128, 256, 512, 1024, 2048};

// This as an attempt to program recognizable "songs" for each alarm level that accomplish
// both informativeness and urgency mapping. The is is to use an index into the buzzer
// level frequencies above, so we can use an unsigned char. We can break the whole
// sequence into 100ms chunks. A 0 will make a "rest" or a silence.a length of 60 will
// give us a 6-second repeat.

const unsigned int NUM_NOTES = 20;
const int SONGS[][NUM_NOTES] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
  { 2, 2, 0, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 2, 2, 0, 0, 0, 0},
  { 3, 3, 3, 0, 3, 3, 3, 3, 0, 3, 3, 3, 0, 3, 3, 3, 0, 0, 0, 0},
  { 4, 0, 4, 0, 4, 0, 4, 0, 0, 0, 4, 0, 4, 0, 4, 0, 4, 0, 0, 0},
  { 4, 4, 2, 0, 4, 4, 2, 0, 4, 4, 2, 0, 4, 4, 2, 0, 4, 4, 2, 0}
};

const int LIGHT_LEVEL[][NUM_NOTES] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
  { 2, 2, 0, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 2, 2, 0, 0, 0, 0},
  { 3, 3, 3, 0, 3, 3, 3, 3, 0, 3, 3, 3, 0, 3, 3, 3, 0, 0, 0, 0},
  { 4, 4, 4, 0, 4, 4, 4, 0, 0, 0, 4, 4, 4, 0, 4, 4, 4, 0, 0, 0},
  { 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5}
};

const unsigned LEN_OF_NOTE_MS = 500;

unsigned long start_of_song = 0;



// in general, we want tones to last forever, although
// I may implement blinking later.
const unsigned long INF_DURATION = 4294967295;

//Allow indexing to LIGHT[] by symbolic name. So LIGHT0 is first and so on.
int LIGHT[] = {LIGHT0, LIGHT1, LIGHT2, LIGHT3, LIGHT4};
int NUM_LIGHTS = sizeof(LIGHT) / sizeof(LIGHT[0]);

Stream* local_ptr_to_serial;


volatile boolean isReceived_SPI;
volatile byte peripheralReceived ;

volatile bool procNewPacket = false;
volatile byte indx = 0;
volatile boolean process;

byte received_signal_raw_bytes[MAX_BUFFER_SIZE];

#define DEBUG 0


void setup_spi()
{
  Serial.println(F("Starting SPI Peripheral."));
  Serial.print(F("Pin for SS: "));
  Serial.println(SS);

  pinMode(BUTTON_PIN, INPUT);              // Setting pin 2 as INPUT
  pinMode(LED_PIN, OUTPUT);                // Setting pin 7 as OUTPUT

  //  SPI.begin();    // IMPORTANT. Do not set SPI.begin for a peripherial device.
  pinMode(SS, INPUT_PULLUP); //Sets SS as input for peripherial
  // Why is this not input?
  pinMode(MOSI, INPUT);    //This works for Peripheral
  pinMode(MISO, OUTPUT);    //try this.
  pinMode(SCK, INPUT);                  //Sets clock as input
#if defined(GPAD)
  SPCR |= _BV(SPE);                       //Turn on SPI in Peripheral Mode
  // turn on interrupts
  SPCR |= _BV(SPIE);

  isReceived_SPI = false;
  SPI.attachInterrupt();                  //Interuupt ON is set for SPI commnucation
#else
#endif
}//end setup()

//ISRs
// This is the original...
// I plan to add an index to this to handle the full message that we intend to receive.
// However, I think this also needs a timeout to handle the problem of getting out of synch.
const int SPI_BYTE_TIMEOUT_MS = 200; // we don't get the next byte this fast, we reset.
volatile unsigned long last_byte_ms = 0;

#if defined(HMWK)
// void IRAM_ATTR ISR() {
//    receive_byte(SPDR);
// }
#elif defined(GPAD) // compile for an UNO, for example...
ISR (SPI_STC_vect)                        //Inerrrput routine function
{
  receive_byte(SPDR);
}//end ISR
#endif



void receive_byte(byte c)
{
  last_byte_ms = millis();
  // byte c = SPDR; // read byte from SPI Data Register
  if (indx < sizeof received_signal_raw_bytes) {
    received_signal_raw_bytes[indx] = c; // save data in the next index in the array received_signal_raw_bytes
    indx = indx + 1;
  }
  if (indx >= sizeof received_signal_raw_bytes) {
    process = true;
  }
}


void updateFromSPI()
{
  if (DEBUG > 0) {
    if (process) {
      Serial.println("process true!");
    }
  }
  if (process)
  {

    AlarmEvent event;
    event.lvl = (AlarmLevel) received_signal_raw_bytes[0];
    for (int i = 0; i < MAX_MSG_LEN; i++) {
      event.msg[i] = (char) received_signal_raw_bytes[1 + i];
    }

    if (DEBUG > 1) {
      Serial.print(F("LVL: "));
      Serial.println(event.lvl);
      Serial.println(event.msg);
    }
    int prevLevel = alarm((AlarmLevel) event.lvl, event.msg, &Serial);
    if (prevLevel != event.lvl) {
      annunciateAlarmLevel(&Serial);
    } else {
      unchanged_anunicateAlarmLevel(&Serial);
    }

    indx = 0;
    process = false;

  }
}

// Have to get a serialport here
void myCallback(byte buttonEvent) {
  switch (buttonEvent) {
    case onPress:
      // Do something...
      local_ptr_to_serial->println(F("onPress"));
      currentlyMuted = !currentlyMuted;
      start_of_song = millis();
      annunciateAlarmLevel(local_ptr_to_serial);
      printAlarmState(local_ptr_to_serial);
      break;
  }
}


void robot_api_setup(Stream *serialport) {
  local_ptr_to_serial = serialport;
  Wire.begin();
  lcd.init();
  serialport->println(F("Clear LCD"));
  clearLCD();
  delay(100);
  serialport->println(F("Start LCD splash"));
  splashLCD();
  serialport->println(F("EndLCD splash"));
  serialport->print(F("Set up GPIO pins: "));
  //  pinMode(SWITCH_MUTE, INPUT_PULLUP); //The SWITCH_MUTE is different on Atmega vs ESP32
  for (int i = 0; i < NUM_LIGHTS; i++) {
    serialport->print(LIGHT[i]);
    serialport->print(", ");
    pinMode(LIGHT[i], OUTPUT);
  }
  serialport->println("");
#if defined(GPAD)
  muteButton.set(SWITCH_MUTE, myCallback);
#endif
  serialport->println(F("end set up GPIO pins"));

  printInstructions(serialport);
  AlarmMessageBuffer[0] = '\0';

  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off at end of setup
}

// This has to be called periodically, at a minimum to handle the mute_button
void robot_api_loop() {
#if defined(GPAD)
  muteButton.poll();
#endif
}

/* Assumes LCD has been initilized
   Turns off Back Light
   Clears display
   Turns on back light.
*/
void clearLCD(void) {
  lcd.noBacklight();
  lcd.clear();
}

//Splash a message so we can tell the LCD is working
void splashLCD(void) {
  lcd.init();                      // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);
  //Line 1
  lcd.print(MODEL_NAME);
  lcd.setCursor(3, 1);
  lcd.print(DEVICE_UNDER_TEST);
  //Line 2
  lcd.setCursor(0, 2);
   lcd.print(PROG_NAME);
  lcd.print(" ");
  lcd.print(FIRMWARE_VERSION);
  //Line 3
  lcd.setCursor(0, 3);
  lcd.print("MAC: ");
  //  lcd.print(myMAC);

}
// TODO: We need to break the message up into strings to render properly
// on the display
void showStatusLCD(AlarmLevel level, bool muted, char *msg) {
  lcd.init();
  lcd.clear();
  // Possibly we don't need the backlight if the level is zero!
  if (level != 0) {
    lcd.backlight();
  } else {
    lcd.noBacklight();
  }

  lcd.print("LVL: ");
  lcd.print(level);
  lcd.print(" - ");
  lcd.print(AlarmNames[level]);

  int msgLineStart = 1;
  lcd.setCursor(0, msgLineStart);
  int len = strlen(AlarmMessageBuffer);
  if (len < 9) {
    if (muted) {
      lcd.print("MUTED! MSG:");
    } else {
      lcd.print("MSG:  ");
    }
    msgLineStart = 2;
  }
  if (strlen(AlarmMessageBuffer) == 0) {
    lcd.print("None.");
  } else {

    char buffer[21] = {0}; // note space for terminator

    size_t len = strlen(msg);      // doesn't count terminator
    size_t blen = sizeof(buffer) - 1; // doesn't count terminator
    size_t i = 0;
    // the actual loop that enumerates your buffer
    for (i = 0; i < (len / blen + 1) && i + msgLineStart < 4; ++i)
    {
      memcpy(buffer, msg + (i * blen), blen);
      local_ptr_to_serial->println(buffer);
      lcd.setCursor(0, i + msgLineStart);
      lcd.print(buffer);
    }
  }
}


// This operation is idempotent if there is no change in the abstract state.
void set_light_level(int lvl) {
  for (int i = 0; i < lvl; i++) {
    digitalWrite(LIGHT[i], HIGH);
  }
  for (int i = lvl; i < NUM_LIGHTS; i++) {
    digitalWrite(LIGHT[i], LOW);
  }
}
void unchanged_anunicateAlarmLevel(Stream* serialport) {
  unsigned long m = millis();
  unsigned long time_in_song = m - start_of_song;
  unsigned char note = time_in_song / (unsigned long) LEN_OF_NOTE_MS;
  //   serialport->print("note: ");
  //   serialport->println(note);
  if (note >= NUM_NOTES) {
    note = 0;
    start_of_song = m;
  }
  unsigned char light_lvl = LIGHT_LEVEL[currentLevel][note];
  set_light_level(light_lvl);
  // TODO: Change this to our device types
#if !defined(HMWK)
  if (!currentlyMuted) {
    unsigned char note_lvl = SONGS[currentLevel][note];

    //   serialport->print("note lvl");
    //   serialport->println(note_lvl);

    tone(TONE_PIN, BUZZER_LVL_FREQ_HZ[note_lvl], INF_DURATION);
  } else {
    noTone(TONE_PIN);
  }
#endif
}
void annunciateAlarmLevel(Stream* serialport) {
  start_of_song = millis();
  unchanged_anunicateAlarmLevel(serialport);
#if !defined(HMWK)
  showStatusLCD(currentLevel, currentlyMuted, AlarmMessageBuffer);
#endif
}
