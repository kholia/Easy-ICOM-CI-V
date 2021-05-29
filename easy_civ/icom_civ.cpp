// CAT (CI-V) based communication with Icom transceivers
//
// Borrowed from https://github.com/thjm/Sketches
//
// Copyright (c) 2016 Hermann-Josef Mathes, MIT License
//
// Copyright (c) 2019 Dhiru Kholia - Minor mods

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#endif

#include "icom_civ.h"

#define DEBUG 1

#include "settings.h"

// Only included for debugging stuff
#include <Stream.h>
#include "Streaming.h"

void cat_print_number(unsigned int hex, unsigned int width)
{
	Serial.print("0x");
	if (hex < 16)
		Serial.print("0");
	if (width > 2) {
		if (hex < 0xfff)
			Serial.print("0");
		if (hex < 0xff)
			Serial.print("0");
	}
	Serial.print(hex, HEX);
}

void cat_print_array(const byte* message, size_t msgLength)
{
	for (size_t i = 0; i < msgLength; ++i) {
		cat_print_number(message[i], 2);
		Serial.print(" ");
	}
	if (msgLength)
		Serial.println();
}


static int getNibble(const byte* s, int i)
{
	byte k = s[i/2];

	if (i % 2 == 0)
		k = k >> 4;
	return k & 0x0f;
}

uint32_t parseFrequency(const byte* message)
{

	static byte ks[] = { 10, 11, 8, 9, 6, 7, 4, 5, 2, 3 };
	uint32_t f = 0;

	for (size_t i = 0; i < sizeof(ks); ++i) {
		byte k = ks[i];
		f = 10 * f + getNibble(message,k);
	}

	return f;
}

bool parseMessage(struct context *ctx)
{
	if (!ctx->rxMsgComplete)
		return false;

#ifdef DEBUG
	// Serial << F("Rx(") << ctx->rxMsgLength << F("): ");
	// cat_print_array(ctx->rxMessage, ctx->rxMsgLength);
#endif
	// reset 'complete' flag
	ctx->rxMsgComplete = false;

	bool err = false;

	if (ctx->rxMessage[0] != INTRO) err = true;
	if (ctx->rxMessage[1] != INTRO) err = true;
	if (ctx->rxMessage[ctx->rxMsgLength-1] != EOM) err = true;
	// message to the rig?
	if (ctx->rxMessage[2] == ctx->rigAddress) {
		// erase it
		Serial << F("Skipped ECHO!") << endl;
		digitalWrite(13, LOW);
		ctx->rxMsgLength = 0;
		return false;
	}
	// message from the rig?
	if (ctx->rxMessage[3] != ctx->rigAddress) err = true;

	if (err) {
		Serial << F("CAT message error!") << endl;
		digitalWrite(13, LOW);
		ctx->rxMsgLength = 0;
		return false;
	}

	bool status = true;

	switch (ctx->rxMessage[4]) {
		case eTRANSFER_FREQ: // transfer operating frequency data
		case eGET_OPERATING_FREQ:
			ctx->myFrequency = parseFrequency(&ctx->rxMessage[4]);
			digitalWrite(13, HIGH);
#ifdef USE_SOFTSERIAL
			Serial.println(ctx->myFrequency);
#endif
			break;

		case eTRANSFER_MODE: // transfer operating mode data
		case eGET_OPERATING_MODE:
			// rxMessage[6] is the passband/filter data
			// 0x01 = wide, 0x02 = normal, 0x03 = narrow
			ctx->myMode = ctx->rxMessage[5];
			digitalWrite(13, HIGH);
#ifdef USE_SOFTSERIAL
			Serial.println(ctx->myMode);
#endif
			break;
		default:
#ifdef USE_SOFTSERIAL
			Serial << F("Message ");
			cat_print_number(ctx->rxMessage[4], 2);
			Serial << F(" ignored!") << endl;
#endif
			digitalWrite(13, LOW);
			status = false;
			break;
	}

	ctx->rxMsgLength = 0;

	return status;
}

bool readFrom(struct context *ctx)
{
	static int nIntro = 0;

	byte ch = ctx->CAT->read();

	// Serial << F("Rx: "); cat_print_number(ch, 2); Serial.println();

	if (ch == INTRO) {
		nIntro++;
		ctx->rxMessage[ctx->rxMsgLength++] = ch;
	}
	if (ch != INTRO && nIntro == 2) {
		ctx->rxMessage[ctx->rxMsgLength++] = ch;
	}
	if (ch == EOM) {
		nIntro = 0;
		ctx->rxMsgComplete = true;
	}
	if (ctx->rxMsgLength > MAXLEN) {
		Serial << F("MSG length error!") << endl;
		ctx->rxMsgComplete = false;
		ctx->rxMsgLength = 0;  // clear message buffer
		nIntro = 0;
	}

	return ctx->rxMsgComplete;
}

bool sendMessage(struct context *ctx, const byte *msg, size_t msgLen)
{
	if (!msgLen)
		return false;

	// Serial << F("Tx(") << msgLen << F("): ");
	// cat_print_array(msg, msgLen);

	for (size_t i = 0; i < msgLen; ++i) {
		// SoftwareSerial will not receive during send
		if (ctx->CAT->available())
			readFrom(ctx);

		// Should return 1 on success (i.e. 1 byte written)
		 if (!ctx->CAT->write(msg[i])) {
			return false;
		}
	}

	return true;
}

/*
 * - Message to Icom:
 *  +------+------+----+----+----+------+
 *  | 0xFE | 0xFE | 58 | E0 | 03 | 0xFD |
 *  +------+------+----+----+----+------+
 *
 *  - Message from Icom:
 *  +------+------+----+----+----+-------+-------+-------+-------+-------+------+
 *  | 0xFE | 0xFE | 00 | 58 | 00 | Data0 | Data1 | Data2 | Data3 | Data4 | 0xFD |
 *  +------+------+----+----+----+-------+-------+-------+-------+-------+------+
 */
bool requestFrequency(struct context *ctx)
{
	int msgLen = 0;
	byte message[8];

	message[msgLen++] = INTRO;
	message[msgLen++] = INTRO;
	message[msgLen++] = ctx->rigAddress;
	message[msgLen++] = MY_ADDRESS;
	message[msgLen++] = eGET_OPERATING_FREQ;
	message[msgLen++] = EOM;

	return sendMessage(ctx, message, msgLen);
}

/*
 * - Message to Icom:
 *  +------+------+----+----+----+------+
 *  | 0xFE | 0xFE | 58 | E0 | 03 | 0xFD |
 *  +------+------+----+----+----+------+
 *
 *  - Message from Icom:
 *  +------+------+----+----+----+------+------+
 *  | 0xFE | 0xFE | 00 | 58 | 01 | mode | 0xFD |
 *  +------+------+----+----+----+------+------+
 */
bool requestMode(struct context *ctx)
{
	int msgLen = 0;
	byte message[8];

	message[msgLen++] = INTRO;
	message[msgLen++] = INTRO;
	message[msgLen++] = ctx->rigAddress;
	message[msgLen++] = MY_ADDRESS;
	message[msgLen++] = eGET_OPERATING_MODE;
	message[msgLen++] = EOM;

	return sendMessage(ctx, message, msgLen);
}


/** Create BCD 'byte' from two characters (digits), no error checking done! */
#define bcd(_h,_l) ((int)_l - '0' + 16 * ((int)_h - '0'))

/*
 *  - Message to Icom:
 *  +------+------+----+----+----+-------+-------+-------+-------+-------+------+
 *  | 0xFE | 0xFE | 58 | E0 | 05 | Data0 | Data1 | Data2 | Data3 | Data4 | 0xFD |
 *  +------+------+----+----+----+-------+-------+-------+-------+-------+------+
 *
 *  - Message from Icom:
 *  +------+------+----+----+------+------+
 *  | 0xFE | 0xFE | E0 | 58 | 0xFB | 0xFD |
 *  +------+------+----+----+------+------+
 */
bool writeFrequency(struct context *ctx, uint32_t frequency)
{
	int msgLen = 0;
	byte message[12];

	message[msgLen++] = INTRO;
	message[msgLen++] = INTRO;
	message[msgLen++] = ctx->rigAddress;
	message[msgLen++] = MY_ADDRESS;
	message[msgLen++] = eSET_OPERATING_FREQ;

	byte *ptr = &message[msgLen];

	for (int i = 0; i < 5; ++i)
		message[msgLen++] = 0;

	// convert the integer frequency into a string of 10 bytes length
	char freq_str[12];
	ultoa(frequency, freq_str, 10);

	// ... with leading zeros
	while (strlen(freq_str) < 10) {
		freq_str[strlen(freq_str)+1] = 0;
		for (int i = strlen(freq_str); i > 0; --i)
			freq_str[i] = freq_str[i-1];
		freq_str[0] = '0';
	}

#ifdef DEBUG
	// Serial << "f= " << freq_str << endl;
	// Serial << "len(f)= " << strlen(freq_str) << endl;
#endif

	*ptr++ = bcd(freq_str[8], freq_str[9]);
	*ptr++ = bcd(freq_str[6], freq_str[7]);
	*ptr++ = bcd(freq_str[4], freq_str[5]);
	*ptr++ = bcd(freq_str[2], freq_str[3]);
	*ptr++ = bcd(freq_str[0], freq_str[1]);

	message[msgLen++] = EOM;

#ifdef DEBUG
	// Serial << F("Tx: ");
	// cat_print_array(message, msgLen);
#endif

	ctx->myFrequency = ILLEGAL_FREQ;

	return sendMessage(ctx, message, msgLen);
}

/*
 *  - Message to Icom:
 *  +------+------+----+----+----+------+------+
 *  | 0xFE | 0xFE | 58 | E0 | 06 | mode | 0xFD |
 *  +------+------+----+----+----+------+------+
 *
 *  - Message from Icom:
 *  +------+------+----+----+------+------+
 *  | 0xFE | 0xFE | E0 | 58 | 0xFB | 0xFD |
 *  +------+------+----+----+------+------+
 */
bool writeMode(struct context *ctx, byte mode)
{
	int msgLen = 0;
	byte message[8];

	message[msgLen++] = INTRO;
	message[msgLen++] = INTRO;
	message[msgLen++] = ctx->rigAddress;
	message[msgLen++] = MY_ADDRESS;
	message[msgLen++] = eSET_OPERATING_MODE;
	message[msgLen++] = mode;
	message[msgLen++] = EOM;

	ctx->myMode = ILLEGAL_MODE;

	return sendMessage(ctx, message, msgLen);
}

/* Get the frequency from internal store. */
uint32_t getFrequency(struct context *ctx)
{
        return ctx->myFrequency;
}

/* Get the mode from internal store. */
byte getMode(struct context *ctx)
{
        return ctx->myMode;
}

/* Return true if a message is complete for parsing. */
bool msgIsComplete(struct context *ctx)
{
        return ctx->rxMsgComplete;
}
