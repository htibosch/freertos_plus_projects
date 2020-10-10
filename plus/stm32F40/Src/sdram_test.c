
extern int lUDPLoggingPrintf( const char *pcFormatString, ... ) __attribute__ ((format (__printf__, 1, 2)));

__attribute__((__noinline__)) static unsigned write_sdram_test32 (volatile unsigned *apPtr, const int aCount, const int aSize)
{
	unsigned sum = 0;
	for (int loop = aCount; loop--; ) {
		volatile unsigned *source = apPtr;
		unsigned toassign = loop;
		for (int index = aSize-8; index >= 8; ) {
			source[0] = toassign; toassign <<= 1;
			source[1] = toassign; toassign <<= 1;
			source[2] = toassign; toassign <<= 1;
			source[3] = toassign; toassign <<= 1;
			source[4] = toassign; toassign ^= 0xa55aa55a;
			source[5] = toassign; toassign <<= 1;
			source[6] = toassign; toassign <<= 1;
			source[7] = toassign;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
		source = apPtr;
		toassign = loop;
		for (int index = aSize-8; index >= 8; ) {
			if (source[0] != toassign) { sum++; } toassign <<= 1;
			if (source[1] != toassign) { sum++; } toassign <<= 1;
			if (source[2] != toassign) { sum++; } toassign <<= 1;
			if (source[3] != toassign) { sum++; } toassign <<= 1;
			if (source[4] != toassign) { sum++; } toassign ^= 0xa55aa55a;
			if (source[5] != toassign) { sum++; } toassign <<= 1;
			if (source[6] != toassign) { sum++; } toassign <<= 1;
			if (source[7] != toassign) { sum++; }
			source += 8;
			index -= 8;
			toassign ^= index;
		}
	}
	return sum;
}

__attribute__((__noinline__)) static unsigned write_sdram_test16 (volatile unsigned short *apPtr, const int aCount, const int aSize)
{
	unsigned sum = 0;
	for (int loop = aCount; loop--; ) {
		volatile unsigned short *source = apPtr;
		unsigned short toassign = loop;
		for (int index = aSize-8; index >= 8; ) {
			source[0] = toassign; toassign <<= 1;
			source[1] = toassign; toassign <<= 1;
			source[2] = toassign; toassign <<= 1;
			source[3] = toassign; toassign <<= 1;
			source[4] = toassign; toassign ^= 0xa55a;
			source[5] = toassign; toassign <<= 1;
			source[6] = toassign; toassign <<= 1;
			source[7] = toassign;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
		source = apPtr;
		toassign = loop;
		for (int index = aSize-8; index >= 8; ) {
			if (source[0] != toassign) { sum++; } toassign <<= 1;
			if (source[1] != toassign) { sum++; } toassign <<= 1;
			if (source[2] != toassign) { sum++; } toassign <<= 1;
			if (source[3] != toassign) { sum++; } toassign <<= 1;
			if (source[4] != toassign) { sum++; } toassign ^= 0xa55a;
			if (source[5] != toassign) { sum++; } toassign <<= 1;
			if (source[6] != toassign) { sum++; } toassign <<= 1;
			if (source[7] != toassign) { sum++; }
			source += 8;
			index -= 8;
			toassign ^= index;
		}
	}
	return sum;
}

__attribute__((__noinline__)) static unsigned write_sdram_test8 (volatile unsigned char *apPtr, const int aCount, const int aSize)
{
	unsigned sum = 0;
	for (int loop = aCount; loop--; ) {
		volatile unsigned char *source = apPtr;
		unsigned char toassign = loop;
		for (int index = aSize-8; index >= 8; ) {
			source[0] = toassign; toassign <<= 1;
			source[1] = toassign; toassign <<= 1;
			source[2] = toassign; toassign <<= 1;
			source[3] = toassign; toassign <<= 1;
			source[4] = toassign; toassign ^= 0x5a;
			source[5] = toassign; toassign <<= 1;
			source[6] = toassign; toassign <<= 1;
			source[7] = toassign;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
		source = apPtr;
		toassign = loop;
		for (int index = aSize-8; index >= 8; ) {
			if (source[0] != toassign) { sum++; } toassign <<= 1;
			if (source[1] != toassign) { sum++; } toassign <<= 1;
			if (source[2] != toassign) { sum++; } toassign <<= 1;
			if (source[3] != toassign) { sum++; } toassign <<= 1;
			if (source[4] != toassign) { sum++; } toassign ^= 0x5a;
			if (source[5] != toassign) { sum++; } toassign <<= 1;
			if (source[6] != toassign) { sum++; } toassign <<= 1;
			if (source[7] != toassign) { sum++; }
			source += 8;
			index -= 8;
			toassign ^= index;
		}
	}
	return sum;
}

void sdram_test()
{
	const unsigned wordCount = 256;
	const int loopCount = 1000;
	unsigned sdramBuf[ wordCount ];
	unsigned errCount32 = write_sdram_test32 ((volatile unsigned *)sdramBuf, loopCount, wordCount);
	unsigned errCount16 = write_sdram_test16 ((volatile unsigned short *)sdramBuf, loopCount, 2u * wordCount);
	unsigned errCount8  = write_sdram_test8 ((volatile unsigned char *)sdramBuf, loopCount, 4u * wordCount);
	lUDPLoggingPrintf ("SDRAM errCount = %u, %u, %u\n", errCount32, errCount16, errCount8);
}
