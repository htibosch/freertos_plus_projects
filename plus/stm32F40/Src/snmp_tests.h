#ifndef SNMP_TESTS_H

#define SNMP_PACKET_COUNT   6

struct SPacket
{
	uint8_t *pucBytes;
	size_t uxLength;
};

extern struct SPacket xSMNPPackets[ SNMP_PACKET_COUNT ];

#endif /* SNMP_TESTS_H */


