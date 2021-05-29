// CAT (CI-V) based communication with ICom transceivers
//
// Borrowed from https://github.com/thjm/Sketches
//
// Copyright (c) 2016 Hermann-Josef Mathes, MIT License
//
// Copyright (c) 2019 Dhiru Kholia - Minor mods

#ifndef __ICOM_CIV_H_
 #define __ICOM_CIV_H_

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#endif

#define MAXLEN 20 /* maximum message length */

/* Structure to hold state variables */
struct context {
        Stream    *CAT;
	byte      rigAddress;
	byte      rxMessage[MAXLEN+1];
	size_t    rxMsgLength;
	bool      rxMsgComplete;
	byte      myMode;
	uint32_t  myFrequency;
};

/* Helpers for the ICom CAT protocol.
 *
 * Cn = command number
 * Sc = sub-command
 *
 * - PC to ICom (IC706MkIIG)
 *
 *  +------+------+----+----+----+----+------+------+
 *  | 0xFE | 0xFE | 58 | E0 | Cn | Sc | Data | 0xFD |
 *  +------+------+----+----+----+----+------+------+
 *
 * - ICom to PC
 *
 *  +------+------+----+----+----+----+------+------+
 *  | 0xFE | 0xFE | E0 | 58 | Cn | Sc | Data | 0xFD |
 *  +------+------+----+----+----+----+------+------+
 *
 * - OK message to PC
 *
 *  +------+------+----+----+------+------+
 *  | 0xFE | 0xFE | E0 | 58 | 0xFB | 0xFD |
 *  +------+------+----+----+------+------+
 *
 * - NG (not good) message to PC
 *
 *  +------+------+----+----+------+------+
 *  | 0xFE | 0xFE | E0 | 58 | 0xFA | 0xFD |
 *  +------+------+----+----+------+------+
 *
 */

/* CAT special message codes. */
enum {
	NG = 0xfa,
	OK = 0xfb,
	COLLISION = 0xfc,
	EOM = 0xfd,
	INTRO = 0xfe,

	MY_ADDRESS = 0xe0,         // standard CIV address of host software

	ILLEGAL_MODE = 0xFF,       // no mode
	ILLEGAL_FREQ = 0xFFFFFFFF, // no frequency
};

/* Enums with known CIT/CAT commands. */
enum {
	eTRANSFER_FREQ       = 0x00,
	eTRANSFER_MODE       = 0x01,
	//eGET_BAND_LIMITS    = 0x02,
	eGET_OPERATING_FREQ  = 0x03,
	eGET_OPERATING_MODE  = 0x04,
	eSET_OPERATING_FREQ  = 0x05,
	eSET_OPERATING_MODE  = 0x06,
};

/* Enums with known rig modes (ICom CIV). */
enum {
	eModeLSB    = 0x00,
	eModeUSB    = 0x01,
	eModeAM     = 0x02,
	eModeCW     = 0x03,
	eModeRTTY   = 0x04,
	eModeFM     = 0x05,
	eModeWFM    = 0x06,
	eModeCW_R   = 0x07,
	eModeRTTY_R = 0x08,
};

bool parseMessage(struct context *ctx);

/* Read one byte from the serial stream and put it into the internal message buffer.
 *
 * @return true if a complete message is in the buffer.
 */
bool readFrom(struct context *ctx);

/* Requesting the frequency from the rig. */
bool requestFrequency(struct context *ctx);

/* Requesting the mode from the rig. */
bool requestMode(struct context *ctx);

/* Write the desired frequency to the rig. */
bool writeFrequency(struct context *ctx, uint32_t frequency);

/* Write the desired mode to the rig. */
bool writeMode(struct context *ctx, byte mode);

/* Parse frequency in BCD format to integer representation. */
uint32_t parseFrequency(const byte* message);

/* Send string of bytes to the rig. */
bool sendMessage(struct context *ctx, const byte *txMsg, size_t msgLen);

/* Create BCD 'byte' from two characters (digits), no error checking done! */
#define bcd(_h,_l) ((int)_l - '0' + 16 * ((int)_h - '0'))

/* Get specified nibble from byte string. */
static int getNibble(const byte* s, int i);

/* Print an integer in hex format, width can be specified, starts always with '0x'. */
static void cat_print_number(unsigned int, unsigned int width);

/* Print a complete char array in hex format, one line. */
static void cat_print_array(const byte* message, size_t msgLength);

/* Get the frequency from internal store. */
uint32_t getFrequency(struct context *ctx);

/* Get the mode from internal store. */
byte getMode(struct context *ctx);

/* Return true if a message is complete for parsing. */
bool msgIsComplete(struct context *ctx);

#endif // __ICOM_CAT_H_
