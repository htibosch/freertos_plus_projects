#!/bin/bash
#
# Compile all sub projects

PLUS_DIR=$HOME/work/plus

for SUB_DIR in  stm32F40/Src stm32F7 xilinx/RTOSDemo sam4e/src same70/src
do
	cd ${PLUS_DIR}/${SUB_DIR}
	echo `pwd`
	make clean
	make -j 8 all
	if [ $? -ne 0 ]
	then
	   echo "make ${SUB_DIR} failed"
	   break
	fi
done

