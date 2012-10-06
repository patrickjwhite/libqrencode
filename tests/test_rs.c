#include <stdio.h>
#include <string.h>
#include "common.h"
#include "../qrencode_inner.h"
#include "../qrinput.h"
#include "../rscode.h"
#include "../qrspec.h"
#include "../mqrspec.h"

/* See pp. 73 of JIS X0510:2004 */
void test_rscode1(void)
{
	QRinput *stream;
	QRRawCode *code;
	static const char str[9] = "01234567";
	static unsigned char correct[26] = {
		0x10, 0x20, 0x0c, 0x56, 0x61, 0x80, 0xec, 0x11, 0xec, 0x11, 0xec, 0x11,
		0xec, 0x11, 0xec, 0x11, 0xa5, 0x24, 0xd4, 0xc1, 0xed, 0x36, 0xc7, 0x87,
		0x2c, 0x55};

	testStart("RS ecc test");
	stream = QRinput_new();
	QRinput_append(stream, QR_MODE_NUM, 8, (unsigned char *)str);
	QRinput_setErrorCorrectionLevel(stream, QR_ECLEVEL_M);
	code = QRraw_new(stream);

	testEnd(memcmp(correct + 16, code->ecccode, 10));
	QRinput_free(stream);
	QRraw_free(code);
}

void calc_maxNroots(void)
{
	int version;
	QRecLevel level;
	int spec[5];
	int maxNroots = 0;
	int nroots;

	for(version = 1; version <= QRSPEC_VERSION_MAX; version++) {
		for(level = QR_ECLEVEL_L; level <= QR_ECLEVEL_H; level++) {
			QRspec_getEccSpec(version, level, spec);
			nroots = QRspec_rsEccCodes1(spec);
			if(nroots > maxNroots) maxNroots = nroots;
			nroots = QRspec_rsEccCodes2(spec);
			if(nroots > maxNroots) maxNroots = nroots;
		}
	}

	for(version = 1; version <= MQRSPEC_VERSION_MAX; version++) {
		for(level = QR_ECLEVEL_L; level <= QR_ECLEVEL_H; level++) {
			nroots = MQRspec_getECCLength(version, level);
			if(nroots > maxNroots) maxNroots = nroots;
		}
	}
	
	printf("Maximum number of nroots: %d\n", maxNroots);
}

int main(void)
{
	test_rscode1();

	report();

	calc_maxNroots();

	return 0;
}
