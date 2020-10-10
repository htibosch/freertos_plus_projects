@echo off
make allsizes | sed "s/\t/ /g;s/  */ /g;s/^ //"
