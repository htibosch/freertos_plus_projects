/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"

#include "snmp_tests.h"

uint8_t ucSNMP_0[] = {
	0x70, 0x5a, 0x0f, 0x83, 0x77, 0x73, 0x54, 0x88, 0xde, 0xe6, 0xf8, 0x46, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x49, 0x6e, 0xce, 0x00, 0x00, 0x74, 0x11, 0x5d, 0xef, 0x0a, 0x3c, 0x00, 0x14, 0x0a, 0x7f,
	0x65, 0x18, 0xf1, 0xe7, 0x00, 0xa1, 0x00, 0x35, 0x64, 0x57, 0x30, 0x2b, 0x02, 0x01, 0x00, 0x04,
	0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02, 0x02, 0x06, 0x24, 0x02, 0x01, 0x00,
	0x02, 0x01, 0x00, 0x30, 0x12, 0x30, 0x10, 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x2b, 0x0b,
	0x01, 0x01, 0x06, 0x01, 0x01, 0x05, 0x00
};
uint8_t ucSNMP_1[] = {
	0x18, 0xdb, 0xf2, 0x56, 0x10, 0x68, 0x54, 0x88, 0xde, 0xe6, 0xf8, 0x46, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x49, 0x8a, 0x55, 0x00, 0x00, 0x74, 0x11, 0x42, 0x66, 0x0a, 0x3c, 0x00, 0x14, 0x0a, 0x7f,
	0x65, 0x1a, 0xf1, 0xe7, 0x00, 0xa1, 0x00, 0x35, 0x60, 0x55, 0x30, 0x2b, 0x02, 0x01, 0x00, 0x04,
	0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02, 0x02, 0x06, 0x28, 0x02, 0x01, 0x00,
	0x02, 0x01, 0x00, 0x30, 0x12, 0x30, 0x10, 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x2b, 0x0b,
	0x01, 0x01, 0x06, 0x01, 0x01, 0x05, 0x00
};
uint8_t ucSNMP_2[] = {
	0x00, 0x60, 0x35, 0x35, 0x5d, 0x9b, 0x54, 0x88, 0xde, 0xe6, 0xf8, 0x46, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x49, 0x9a, 0x8b, 0x00, 0x00, 0x74, 0x11, 0x32, 0x12, 0x0a, 0x3c, 0x00, 0x14, 0x0a, 0x7f,
	0x65, 0x38, 0xf1, 0xe7, 0x00, 0xa1, 0x00, 0x35, 0x24, 0x37, 0x30, 0x2b, 0x02, 0x01, 0x00, 0x04,
	0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02, 0x02, 0x06, 0x64, 0x02, 0x01, 0x00,
	0x02, 0x01, 0x00, 0x30, 0x12, 0x30, 0x10, 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x2b, 0x0b,
	0x01, 0x01, 0x06, 0x01, 0x01, 0x05, 0x00
};
uint8_t ucSNMP_3[] = {
	0x00, 0x60, 0x35, 0x35, 0x5d, 0x96, 0x54, 0x88, 0xde, 0xe6, 0xf8, 0x46, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x49, 0x09, 0xf5, 0x00, 0x00, 0x74, 0x11, 0xc2, 0x9e, 0x0a, 0x3c, 0x00, 0x14, 0x0a, 0x7f,
	0x65, 0x42, 0xf1, 0xe7, 0x00, 0xa1, 0x00, 0x35, 0x10, 0x2d, 0x30, 0x2b, 0x02, 0x01, 0x00, 0x04,
	0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02, 0x02, 0x06, 0x78, 0x02, 0x01, 0x00,
	0x02, 0x01, 0x00, 0x30, 0x12, 0x30, 0x10, 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x2b, 0x0b,
	0x01, 0x01, 0x06, 0x01, 0x01, 0x05, 0x00
};
uint8_t ucSNMP_4[] = {
	0x98, 0xaf, 0x65, 0xc2, 0xa6, 0x37, 0x54, 0x88, 0xde, 0xe6, 0xf8, 0x46, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x49, 0x9a, 0x8a, 0x00, 0x00, 0x74, 0x11, 0x32, 0x03, 0x0a, 0x3c, 0x00, 0x14, 0x0a, 0x7f,
	0x65, 0x48, 0xf1, 0xe7, 0x00, 0xa1, 0x00, 0x35, 0x04, 0x27, 0x30, 0x2b, 0x02, 0x01, 0x00, 0x04,
	0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02, 0x02, 0x06, 0x84, 0x02, 0x01, 0x00,
	0x02, 0x01, 0x00, 0x30, 0x12, 0x30, 0x10, 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x2b, 0x0b,
	0x01, 0x01, 0x06, 0x01, 0x01, 0x05, 0x00
};
uint8_t ucSNMP_5[] = {
	0x00, 0x60, 0x35, 0x35, 0x12, 0x84, 0x54, 0x88, 0xde, 0xe6, 0xf8, 0x46, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x49, 0x16, 0xb0, 0x00, 0x00, 0x74, 0x11, 0xb5, 0xe4, 0x0a, 0x3c, 0x00, 0x14, 0x0a, 0x7f,
	0x65, 0x41, 0xf1, 0xe7, 0x00, 0xa1, 0x00, 0x35, 0x12, 0x2e, 0x30, 0x2b, 0x02, 0x01, 0x00, 0x04,
	0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02, 0x02, 0x06, 0x76, 0x02, 0x01, 0x00,
	0x02, 0x01, 0x00, 0x30, 0x12, 0x30, 0x10, 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x2b, 0x0b,
	0x01, 0x01, 0x06, 0x01, 0x01, 0x05, 0x00
};

struct SPacket xSMNPPackets[] = {
	{ ucSNMP_0, sizeof ucSNMP_0 },
	{ ucSNMP_1, sizeof ucSNMP_1 },
	{ ucSNMP_2, sizeof ucSNMP_2 },
	{ ucSNMP_3, sizeof ucSNMP_3 },
	{ ucSNMP_4, sizeof ucSNMP_4 },
	{ ucSNMP_5, sizeof ucSNMP_5 },
};