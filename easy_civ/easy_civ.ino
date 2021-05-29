// Test program for CAT (CI-V) based communication with ICom transceivers
//
// Borrowed from https://github.com/thjm/Sketches
//
// Copyright (c) 2016 Hermann-Josef Mathes, MIT License
//
// Copyright (c) 2019 Dhiru Kholia - Minor mods

#include "Arduino.h"
#include "settings.h"

#ifdef USE_SOFTSERIAL
#include <SoftwareSerial.h>
SoftwareSerial rxSerial(10, 11); // RX, TX for the RX
#else
#define rxSerial Serial
#endif

#include "icom_civ.h"

// #include "TimeLib.h"

#define DEBUG 1

// Only included for debugging stuff
#include <Stream.h>
#include "Streaming.h"

/* Address definition for the ICom706MkIIG transceiver */
uint8_t const kIC706MK2G_ADDR = 0x58;

/* Default RX operating frequency. */
uint32_t const kRxFrequencyDefault = 7175000;

/*d* Minimum RX operating frequency. */
uint32_t const kRxFrequencyMin = 7000000;

/* Maximum RX operating frequency. */
uint32_t const kRxFrequencyMax = 7200000;

#define UART_BAUD_RATE 9600

/* Return true if the frequency is in the desired range. */
bool rxFrequencyOK(struct context *ctx)
{
  uint32_t frequency = getFrequency(ctx);

  // if (frequency == ILLEGAL_FREQ) return false;
  if (frequency < kRxFrequencyMin) return false;
  if (frequency >= kRxFrequencyMax) return false;

  return true;
}

/* Return true if the mode is either USB or CW. */
bool rxModeOK(struct context *ctx)
{
  byte mode = getMode(ctx);

  // if (mode == ILLEGAL_MODE) return false;
  if (mode == eModeUSB) return true;
  if (mode == eModeCW) return true;

  return false;
}

/* This is active if either mode or frequency haven't been read from the rig. */
static void rxRequestFrequencyAndMode(struct context *ctx, uint32_t frequency, byte mode)
{
  static unsigned long r_time = millis();

  if (frequency == ILLEGAL_FREQ && (millis() - r_time) > 100) {
    requestFrequency(ctx);
    r_time = millis();
  }

  if (mode == ILLEGAL_MODE && (millis() - r_time) > 100) {
    requestMode(ctx);
    r_time = millis();
  }
}

/* Called when the frequency is not in the right range. */
static void rxSetFrequency(struct context *ctx)
{
  static unsigned long s_time = millis();

  if (!rxFrequencyOK(ctx) && (millis() - s_time) > 200) {
    writeFrequency(ctx, kRxFrequencyDefault);
    s_time = millis();
  }
}

/* Called when the mode is not correct. */
static void rxSetMode(struct context *ctx)
{
  static unsigned long s_time = millis();

  if (!rxModeOK(ctx) && (millis() - s_time) > 200) {
    writeMode(ctx, eModeUSB);
    s_time = millis();
  }
}

struct context icom;

// #define DEMO_MODE 1 /* NOTE */

// code for setup and initialisation
void setup()
{
  pinMode(13, OUTPUT);

#ifndef DEMO_MODE

  icom.CAT  = &rxSerial;
  icom.rigAddress = kIC706MK2G_ADDR;
  icom.rxMsgLength = 0;
  icom.rxMsgComplete = false;
  icom.myMode = ILLEGAL_MODE;
  icom.myFrequency = ILLEGAL_FREQ;

  /* Initialize serial output at UART_BAUD_RATE bps */
  Serial.begin(UART_BAUD_RATE);

#ifdef USE_SOFTSERIAL
  Serial << F("CATtransv: starting ...") << endl;
#endif
  // set the data rate for the SoftwareSerial port(s)
  rxSerial.begin(UART_BAUD_RATE);
#endif
  // setSyncProvider(requestSync);  // set function to call when sync required
}

// main loop, program logic:
// - check RX:
//   - if frequency in range 70000 .. 70200 kHz -> OK
//     else change to 70175
//   - if mode USB -> OK, else change to USB
// - set TX:
//   - mode = RX mode
//   - frequency = ...

void loop()
{
#ifndef DEMO_MODE
  static unsigned long loop_init = millis();
  static unsigned long rx_init = millis();

  byte mode = getMode(&icom);
  uint32_t frequency = getFrequency(&icom);

  static byte rxState = 0;

  // set the rxState appropriately
  if (frequency == ILLEGAL_FREQ || mode == ILLEGAL_MODE)
    rxState = 0;
  else if (!rxFrequencyOK(&icom))
    rxState = 1;
  else if (!rxModeOK(&icom))
    rxState = 2;
  else
    rxState = 3;

  switch (rxState) {
    case 0:
      rxRequestFrequencyAndMode(&icom, frequency, mode);
      break;

    case 1:
      rxSetFrequency(&icom);  // set right frequency
      break;

    case 2:
      rxSetMode(&icom);  // set right mode
      break;

    case 3:
      // writeFrequency(&icom, kRxFrequencyDefault);
      // writeMode(&icom, eModeCW);
      if ((millis() - rx_init) > 5000) {
        rxRequestFrequencyAndMode(&icom, frequency, mode);
        rx_init = millis();
      }
      break;

    default:
      break;
  }

#ifdef USE_SOFTSERIAL
  rxSerial.listen();
#endif

  // check if 'rxSerial' has data to receive
  if (rxSerial.available()) {
    readFrom(&icom);

    if (msgIsComplete(&icom)) {
      parseMessage(&icom);
    }
  }

  // display of frequency and mode from time to time
  /* if (millis() - loop_init > 2000) {
  	loop_init = millis();

  	Serial << F("OP mode ") << rxState
  		<< F(" fOK ") << rxFrequencyOK(icom)
  		<< F(" mOK ") << rxModeOK(icom)
  		<< F(": Frequency = ") << frequency
  		<< F(" Mode = ") << mode << endl;
    } */
#endif

  /* if (Serial.available()) {
  	processSyncMessage();

  	if (timeStatus()!= timeNotSet) {
  		digitalClockDisplay();
  	}
  	if (timeStatus() == timeSet) {
  		digitalWrite(13, HIGH); // LED on if synced
  		setDS3231time(second(), minute(), hour(), weekday(), day(), month(), year() % 2000);
  	} else {
  		digitalWrite(13, LOW);  // LED off if needs refresh
  	}
    }
    delay(2000);

    raw_mode_demo(); */
}
