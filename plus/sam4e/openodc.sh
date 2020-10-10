
ELF=${HOME}/work/plus/sam4e/src/RTOSDemo.elf
SCRIPTS=${HOME}/opt/xPacks/openocd/0.10.0-13/scripts
CFG_1=interface/cmsis-dap.cfg
CFG_2=board/atmel_sam4e_ek.cfg
CMDS="program ${ELF} verify reset exit"

echo ${ELF}
echo ${SCRIPTS}
echo ${CFG_1}
echo ${CFG_2}
echo ${CMDS}

${HOME}/opt/xPacks/openocd/0.10.0-13/bin/openocd -s ${SCRIPTS} -f ${CFG_1} -f ${CFG_2} -c "${CMDS}"

# sudo openocd -f interface/cmsis-dap.cfg -f board/atmel_sam4e_ek.cfg -c "program image.elf verify reset exit"
