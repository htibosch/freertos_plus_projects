echo text	data	bss	dec	hex	filename > allsizes.txt
C:\Ac6\7.2.1-1.1-20180401-0515\bin\arm-none-eabi-size.exe stm32F40.elf | sed "s/\t/ /g;s/  */ /g;s/^ //" > allsizes.txt
rem make allsizes | grep -i -h FreeRTOS-Plus-TCP | sed "s/\t/ /g;s/  */ /g;s/^ //" >> allsizes.txt
make allsizes | sed "s/\t/ /g;s/  */ /g;s/^ //" >> allsizes.txt
