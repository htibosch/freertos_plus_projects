# Hey Emacs, this is a -*- makefile -*-

# Goals available on make command line:
#
# [all]                   Default goal: build the project.
# clean                   Clean up the project.
# rebuild                 Rebuild the project.
# ccversion               Display CC version information.
# cppfiles  file.i        Generate preprocessed files from C source files.
# asfiles   file.x        Generate preprocessed assembler files from C and assembler source files.
# objfiles  file.o        Generate object files from C and assembler source files.
# a         file.a        Archive: create A output file from object files.
# elf       file.elf      Link: create ELF output file from object files.
# lss       file.lss      Create extended listing from target output file.
# sym       file.sym      Create symbol table from target output file.
# hex       file.hex      Create Intel HEX image from ELF output file.
# bin       file.bin      Create binary image from ELF output file.
# sizes                   Display target size information.
# isp                     Use ISP instead of JTAGICE mkII when programming.
# cpuinfo                 Get CPU information.
# halt                    Stop CPU execution.
# chiperase               Perform a JTAG Chip Erase command.
# erase                   Perform a flash chip erase.
# program                 Program MCU memory from ELF output file.
# secureflash             Protect chip by setting security bit.
# reset                   Reset MCU.
# debug                   Open a debug connection with the MCU.
# run                     Start CPU execution.
# readregs                Read CPU registers.
# doc                     Build the documentation.
# cleandoc                Clean up the documentation.
# rebuilddoc              Rebuild the documentation.
# verbose                 Display main executed commands.

# ** ** ** *** ** ** ** ** ** ** ** ** ** ** **
# ENVIRONMENT SETTINGS
# ** ** ** *** ** ** ** ** ** ** ** ** ** ** **

FirstWord = $(if $(1),$(word 1,$(1)))
LastWord  = $(if $(1),$(word $(words $(1)),$(1)))

MAKE      = make
MAKECFG   = config.mk
TGTTYPE   = $(suffix $(TARGET))
TGTNAME   = $(prefix $(TARGET))
TGTFILE   = $(TARGET)

RM        = rm -Rf

AR        = $(GCC_BIN)/$(GCC_PREFIX)-ar
ARFLAGS   = rcs

# ==========================================================================
CPP       = $(CC) -E

CPP_ONLY_FLAGS = -fno-rtti -fno-exceptions

CPPFLAGS  = $(HW_FLAGS) $(WARNINGS) \
            $(PLATFORM_INC_PATH:%=-I%) $(INC_PATH:%=-I%) $(CPP_EXTRA_FLAGS)
DPNDFILES = $(CPP_SRCS:.cpp=.d) $(C_SRCS:.c=.d) $(S_SRCS:.S=.d)
CPPFILES  = $(CPP_SRCS:.cpp=.i) $(C_SRCS:.c=.i)


GPP=$(GCC_BIN)/$(GCC_PREFIX)-g++

CC        = $(GCC_BIN)/$(GCC_PREFIX)-gcc
CFLAGS    = $(DEBUG) $(OPTIMIZATION) $(C_EXTRA_FLAGS) $(DEFS) \
            $(AS_EXTRA_FLAGS)
ASFILES   = $(C_SRCS:.c=.x) $(CPP_SRCS:.cpp=.x) $(S_SRCS:.S=.x)

AS        = $(GCC_BIN)/$(GCC_PREFIX)-as
ASFLAGS   = $(DEBUG) \
            $(PLATFORM_INC_PATH:%=-Wa,-I%) $(INC_PATH:%=-Wa,-I%) $(AS_EXTRA_FLAGS)
OBJFILES  = $(S_SRCS:.S=.o) $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)


STRINGS   = $(GCC_BIN)/$(GCC_PREFIX)-strings.exe

ifeq ($(USE_CPP),true)
LD        = $(CC)
else
LD        = $(CC)
endif

# LDFLAGS   = $(LIB_PATH:%=-L%) -Xlinker --gc-sections -Wl,-T -Wl,$(LINKER_SCRIPT) $(LD_EXTRA_FLAGS)
LDFLAGS   = $(LIB_PATH:%=-L%) -Xlinker --gc-sections -T $(LINKER_SCRIPT) $(LD_EXTRA_FLAGS)

#LDLIBS    = $(LIBS:%=-l%)


OBJDUMP   = $(GCC_BIN)/$(GCC_PREFIX)-objdump
LSS       = $(TGTFILE:$(TGTTYPE)=.lss)

NM        = $(GCC_BIN)/$(GCC_PREFIX)-nm
SYM       = $(TGTFILE:$(TGTTYPE)=.sym)

OBJCOPY   = $(GCC_BIN)/$(GCC_PREFIX)-objcopy
HEX       = $(TGTFILE:$(TGTTYPE)=.hex)
BIN       = $(TGTFILE:$(TGTTYPE)=.bin)

SIZE      = $(GCC_BIN)/$(GCC_PREFIX)-size

SLEEP     = sleep
SLEEPUSB  = 9

ISP       = batchisp
ISPFLAGS  = -device -hardware usb -operation

DBGPROXY  = gdbproxy

DOCGEN    = doxygen

# ** ** ** *** ** ** ** ** ** ** ** ** ** ** **
# MESSAGES
# ** ** ** *** ** ** ** ** ** ** ** ** ** ** **

ERR_TARGET_TYPE       = Target type not supported: `$(TGTTYPE)'
MSG_CLEANING          = Cleaning project.
MSG_PREPROCESSING     = Preprocessing \`$<\' to \`$@\'.
MSG_COMPILING         = Compiling \`$<\' to \`$@\'.
MSG_ASSEMBLING        = Assembling \`$<\' to \`$@\'.
MSG_ARCHIVING         = Archiving to \`$@\'.
MSG_LINKING           = Linking to \`$@\'.
MSG_EXTENDED_LISTING  = Creating extended listing to \`$@\'.
MSG_SYMBOL_TABLE      = Creating symbol table to \`$@\'.
MSG_TEXT_TABLE        = Creating map of variables
MSG_STRING_TABLE      = Creating list of strings
MSG_IHEX_IMAGE        = Creating Intel HEX image to \`$@\'.
MSG_BINARY_IMAGE      = Creating binary image to \`$@\'.
MSG_GETTING_CPU_INFO  = Getting CPU information.
MSG_HALTING           = Stopping CPU execution.
MSG_ERASING_CHIP      = Performing a JTAG Chip Erase command.
MSG_ERASING           = Performing a flash chip erase.
MSG_SECURING_FLASH    = Protecting chip by setting security bit.
MSG_RESETTING         = Resetting MCU.
MSG_DEBUGGING         = Opening debug connection with MCU.
MSG_RUNNING           = Starting CPU execution.
MSG_READING_CPU_REGS  = Reading CPU registers.
MSG_CLEANING_DOC      = Cleaning documentation.
MSG_GENERATING_DOC    = Generating documentation to \`$(DOC_PATH)\'.

ECHO_CMD=/bin/echo
TOUCH_CMD=/usr/bin/touch

# ** ** ** *** ** ** ** ** ** ** ** ** ** ** **
# MAKE RULES
# ** ** ** *** ** ** ** ** ** ** ** ** ** ** **

# Include the make configuration file.
include $(MAKECFG)

# ** ** TOP-LEVEL RULES ** **

# Default goal: build the project.
ifeq ($(TGTTYPE),.a)
.PHONY: all
all: ccversion a lss sym sizes
else
ifeq ($(TGTTYPE),.elf)
.PHONY: all
all: ccversion elf lss sym hex bin sizes
else
$(error $(ERR_TARGET_TYPE))
endif
endif

# Clean up the project.
.PHONY: clean
clean:
	@$(ECHO_CMD) $(MSG_CLEANING)
	-$(VERBOSE_CMD)$(RM) $(BIN)
	-$(VERBOSE_CMD)$(RM) $(HEX)
	-$(VERBOSE_CMD)$(RM) $(SYM)
	-$(VERBOSE_CMD)$(RM) $(LSS)
	-$(VERBOSE_CMD)$(RM) $(TGTFILE)
	-$(VERBOSE_CMD)$(RM) $(OBJFILES)
	-$(VERBOSE_CMD)$(RM) $(ASFILES)
	-$(VERBOSE_CMD)$(RM) $(CPPFILES)
	-$(VERBOSE_CMD)$(RM) $(DPNDFILES)
	$(VERBOSE_NL)

# Rebuild the project.
.PHONY: rebuild
rebuild: clean all

# Display CC version information.
.PHONY: ccversion
ccversion: $(EARLY_TARGETS)
	$(CC) --version

# Generate preprocessed files from C source files.
.PHONY: cppfiles
cppfiles: $(CPPFILES)

# Generate preprocessed assembler files from C and assembler source files.
.PHONY: asfiles
asfiles: $(ASFILES)

# Generate object files from C and assembler source files.
.PHONY: objfiles
objfiles: $(OBJFILES)

ifeq ($(TGTTYPE),.a)
# Archive: create A output file from object files.
.PHONY: a
a: $(TGTFILE)
else
ifeq ($(TGTTYPE),.elf)
# Link: create ELF output file from object files.
.PHONY: elf
elf: $(TGTFILE)
endif
endif

# Create extended listing from target output file.
.PHONY: lss
lss: $(LSS)

# Create symbol table from target output file.
.PHONY: sym
sym: $(SYM)

ifeq ($(TGTTYPE),.elf)

# Create Intel HEX image from ELF output file.
.PHONY: hex
hex: $(HEX)

# Create binary image from ELF output file.
.PHONY: bin
bin: $(BIN)

endif

# Display target size information.
.PHONY: sizes
sizes: $(TGTFILE)
	@$(ECHO_CMD) .
ifeq ($(TGTTYPE),.a)
	@$(SIZE) -Bxt $<
else
ifeq ($(TGTTYPE),.elf)
	@$(SIZE) -Ax $<
	@$(SIZE) -Bx $<
endif
endif
	@$(ECHO_CMD) .
	@$(ECHO_CMD) .

ifeq ($(TGTTYPE),.elf)

# Use ISP instead of JTAGICE mkII when programming.
.PHONY: isp
ifeq ($(filter-out isp verbose,$(MAKECMDGOALS)),)
isp: all
else
isp:
	@:
endif

ifeq ($(findstring isp,$(MAKECMDGOALS)),)

# Get CPU information.
.PHONY: cpuinfo
cpuinfo:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_GETTING_CPU_INFO)
	$(VERBOSE_CMD)$(PROGRAM) cpuinfo
ifneq ($(call LastWord,$(filter cpuinfo chiperase erase program secureflash reset debug run readregs,$(MAKECMDGOALS))),cpuinfo)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif

# Stop CPU execution.
.PHONY: halt
halt:
ifeq ($(filter cpuinfo chiperase erase program secureflash reset run readregs,$(MAKECMDGOALS)),)
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_HALTING)
	$(VERBOSE_CMD)$(PROGRAM) halt
ifneq ($(call LastWord,$(filter halt debug,$(MAKECMDGOALS))),halt)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif
else
	@:
endif

# Perform a JTAG Chip Erase command.
.PHONY: chiperase
chiperase:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_ERASING_CHIP)
	$(VERBOSE_CMD)$(PROGRAM) chiperase
ifneq ($(call LastWord,$(filter cpuinfo chiperase program secureflash reset debug run readregs,$(MAKECMDGOALS))),chiperase)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif

# Perform a flash chip erase.
.PHONY: erase
erase:
ifeq ($(filter chiperase program,$(MAKECMDGOALS)),)
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_ERASING)
	$(VERBOSE_CMD)$(PROGRAM) erase $(FLASH:%=-f%)
ifneq ($(call LastWord,$(filter cpuinfo erase secureflash reset debug run readregs,$(MAKECMDGOALS))),erase)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif
else
	@:
endif

# Program MCU memory from ELF output file.
.PHONY: program
program: all
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_PROGRAMMING)
	$(VERBOSE_CMD)$(PROGRAM) program $(FLASH:%=-f%) $(PROG_CLOCK:%=-c%) -e -v -R $(if $(findstring run,$(MAKECMDGOALS)),-r) $(TGTFILE)
ifneq ($(call LastWord,$(filter cpuinfo chiperase program secureflash debug readregs,$(MAKECMDGOALS))),program)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif

# Protect chip by setting security bit.
.PHONY: secureflash
secureflash:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_SECURING_FLASH)
	$(VERBOSE_CMD)$(PROGRAM) secureflash
ifneq ($(call LastWord,$(filter cpuinfo chiperase erase program secureflash reset debug run readregs,$(MAKECMDGOALS))),secureflash)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif

# Reset MCU.
.PHONY: reset
reset:
ifeq ($(filter program run,$(MAKECMDGOALS)),)
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_RESETTING)
	$(VERBOSE_CMD)$(PROGRAM) reset
ifneq ($(call LastWord,$(filter cpuinfo chiperase erase secureflash reset debug readregs,$(MAKECMDGOALS))),reset)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif
else
	@:
endif

# Open a debug connection with the MCU.
.PHONY: debug
debug:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_DEBUGGING)
	$(VERBOSE_CMD)$(DBGPROXY) $(FLASH:%=-f%)
ifneq ($(call LastWord,$(filter cpuinfo halt chiperase erase program secureflash reset debug run readregs,$(MAKECMDGOALS))),debug)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif

# Start CPU execution.
.PHONY: run
run:
ifeq ($(findstring program,$(MAKECMDGOALS)),)
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_RUNNING)
	$(VERBOSE_CMD)$(PROGRAM) run $(if $(findstring reset,$(MAKECMDGOALS)),-R)
ifneq ($(call LastWord,$(filter cpuinfo chiperase erase secureflash debug run readregs,$(MAKECMDGOALS))),run)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif
else
	@:
endif

# Read CPU registers.
.PHONY: readregs
readregs:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_READING_CPU_REGS)
	$(VERBOSE_CMD)$(PROGRAM) readregs
ifneq ($(call LastWord,$(filter cpuinfo chiperase erase program secureflash reset debug run readregs,$(MAKECMDGOALS))),readregs)
	@$(SLEEP) $(SLEEPUSB)
else
	@$(ECHO_CMD) .
endif

else

# Perform a flash chip erase.
.PHONY: erase
erase:
ifeq ($(findstring program,$(MAKECMDGOALS)),)
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_ERASING)
	$(VERBOSE_CMD)$(ISP) $(ISPFLAGS) erase f memory flash blankcheck
ifeq ($(call LastWord,$(filter erase secureflash debug run,$(MAKECMDGOALS))),erase)
	@$(ECHO_CMD) .
endif
else
	@:
endif

# Program MCU memory from ELF output file.
.PHONY: program
program: all
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_PROGRAMMING)
	$(VERBOSE_CMD)$(ISP) $(ISPFLAGS) erase f memory flash blankcheck loadbuffer $(TGTFILE) program verify $(if $(findstring run,$(MAKECMDGOALS)),$(if $(findstring secureflash,$(MAKECMDGOALS)),,start $(if $(findstring reset,$(MAKECMDGOALS)),,no)reset 0))
ifeq ($(call LastWord,$(filter program secureflash debug,$(MAKECMDGOALS))),program)
	@$(ECHO_CMD) .
endif

# Protect chip by setting security bit.
.PHONY: secureflash
secureflash:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_SECURING_FLASH)
	$(VERBOSE_CMD)$(ISP) $(ISPFLAGS) memory security addrange 0x0 0x0 fillbuffer 0x01 program $(if $(findstring run,$(MAKECMDGOALS)),start $(if $(findstring reset,$(MAKECMDGOALS)),,no)reset 0)
ifeq ($(call LastWord,$(filter erase program secureflash debug,$(MAKECMDGOALS))),secureflash)
	@$(ECHO_CMD) .
endif

# Reset MCU.
.PHONY: reset
reset:
	@:

# Open a debug connection with the MCU.
.PHONY: debug
debug:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_DEBUGGING)
	$(VERBOSE_CMD)$(DBGPROXY) $(FLASH:%=-f%)
ifeq ($(call LastWord,$(filter erase program secureflash debug run,$(MAKECMDGOALS))),debug)
	@$(ECHO_CMD) .
endif

# Start CPU execution.
.PHONY: run
run:
ifeq ($(filter program secureflash,$(MAKECMDGOALS)),)
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_RUNNING)
	$(VERBOSE_CMD)$(ISP) $(ISPFLAGS) start $(if $(findstring reset,$(MAKECMDGOALS)),,no)reset 0
ifeq ($(call LastWord,$(filter erase debug run,$(MAKECMDGOALS))),run)
	@$(ECHO_CMD) .
endif
else
	@:
endif

endif

endif

# Build the documentation.
.PHONY: doc
doc:
	@$(ECHO_CMD) .
	@$(ECHO_CMD) $(MSG_GENERATING_DOC)
	$(VERBOSE_CMD)cd $(dir $(DOC_CFG)) && $(DOCGEN) $(notdir $(DOC_CFG))
	@$(ECHO_CMD) .

# Clean up the documentation.
.PHONY: cleandoc
cleandoc:
	@$(ECHO_CMD) $(MSG_CLEANING_DOC)
	-$(VERBOSE_CMD)$(RM) $(DOC_PATH)
	$(VERBOSE_NL)

# Rebuild the documentation.
.PHONY: rebuilddoc
rebuilddoc: cleandoc doc

# Display main executed commands.
.PHONY: verbose
ifeq ($(filter-out isp verbose,$(MAKECMDGOALS)),)
verbose: all
else
verbose:
	@:
endif
ifneq ($(findstring verbose,$(MAKECMDGOALS)),)
# Prefix displaying the following command if and only if verbose is a goal.
VERBOSE_CMD =
# New line displayed if and only if verbose is a goal.
VERBOSE_NL  = @$(ECHO_CMD) .
else
VERBOSE_CMD = @
VERBOSE_NL  =
endif

# ** ** COMPILATION RULES ** **

# Include silently the dependency files.
-include $(DPNDFILES)

# The dependency files are not built alone but along with first generation files.
$(DPNDFILES):

# First generation files depend on make files.
$(CPPFILES) $(ASFILES) $(OBJFILES): Makefile $(MAKECFG)

ifeq ($(TGTTYPE),.elf)
# Files resulting from linking depend on linker script.
$(TGTFILE): $(LINKER_SCRIPT)
endif

# ==========================================================================
# C - source
# ==========================================================================
# Preprocess: create preprocessed files from C source files.
%.i: %.c %.d
	@$(ECHO_CMD) $(MSG_PREPROCESSING)
	$(VERBOSE_CMD)$(CPP) $(CPPFLAGS) -MMD -MP -MT $*.i -MT $*.x -MT $*.o $(C_ONLY_FLAGS) -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

# Preprocess & compile: create assembler files from C source files.
%.x: %.c %.d
	@$(ECHO_CMD) $(MSG_COMPILING)
	$(VERBOSE_CMD)$(CC) -S $(CPPFLAGS) -MMD -MP -MT $*.i -MT $*.x -MT $*.o $(CFLAGS) $(C_ONLY_FLAGS) -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)
	
# Preprocess, compile & assemble: create object files from C source files.
%.o: %.c %.d
	@$(ECHO_CMD) $(MSG_COMPILING)
	$(VERBOSE_CMD)$(CC) -c $(CPPFLAGS) -MMD -MP -MT $*.i -MT $*.x -MT $*.o $(CFLAGS) $(C_ONLY_FLAGS) -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

# ==========================================================================
# C++ - source
# ==========================================================================

# Preprocess: create preprocessed files from C source files.
%.i: %.cpp %.d
	@$(ECHO_CMD) $(MSG_PREPROCESSING)
	$(VERBOSE_CMD)$(CPP) $(CPPFLAGS) $(CPP_ONLY_FLAGS) -MMD -MP -MT $*.i -MT $*.x -MT $*.o -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

# Preprocess & compile: create assembler files from C source files.
%.x: %.cpp %.d
	@$(ECHO_CMD) $(MSG_COMPILING)
	$(VERBOSE_CMD)$(CC) -S $(CPPFLAGS) $(CPP_ONLY_FLAGS) -MMD -MP -MT $*.i -MT $*.x -MT $*.o $(CFLAGS) -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

# Preprocess, compile & assemble: create object files from C source files.
%.o: %.cpp %.d
	@$(ECHO_CMD) $(MSG_COMPILING)
	$(VERBOSE_CMD)$(GPP) -c $(CPPFLAGS) $(CPP_ONLY_FLAGS) -MMD -MP -MT $*.i -MT $*.x -MT $*.o $(CFLAGS) -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

# ==========================================================================
# Assembler - source
# ==========================================================================

# Preprocess: create preprocessed files from assembler source files.
%.x: %.S %.d
	@$(ECHO_CMD) $(MSG_PREPROCESSING)
	$(VERBOSE_CMD)$(CPP) $(CPPFLAGS) $(DEFS) -MMD -MP -MT $*.x -MT $*.o -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

# Preprocess & assemble: create object files from assembler source files.
%.o: %.S %.d
	@$(ECHO_CMD) $(MSG_ASSEMBLING)
	$(VERBOSE_CMD)$(CC) -c $(CPPFLAGS) $(DEFS) -MMD -MP -MT $*.x $(ASFLAGS) -o $@ $<
	@$(TOUCH_CMD) $*.d
	@$(TOUCH_CMD) $@
	$(VERBOSE_NL)

.PRECIOUS: $(OBJFILES)
ifeq ($(TGTTYPE),.a)
# Archive: create A output file from object files.
.SECONDARY: $(TGTFILE)
$(TGTFILE): $(OBJFILES)
	@$(ECHO_CMD) $(MSG_ARCHIVING)
	$(VERBOSE_CMD)$(AR) $(ARFLAGS) $@ $(filter %.o,$+)
	$(VERBOSE_NL)
else
ifeq ($(TGTTYPE),.elf)
# Link: create ELF output file from object files.
.SECONDARY: $(TGTFILE)
$(TGTFILE): $(OBJFILES)
	@$(ECHO_CMD) $(MSG_LINKING)
	$(VERBOSE_CMD)$(CC) $(LDFLAGS) $(filter %.o,$+) $(LDLIBS) -o $@
	$(VERBOSE_NL)
endif
endif

.PHONY: allsizes
allsizes: $(TGTFILE)
	$(SIZE) $(OBJFILES)

# Create extended listing from target output file.
#$(LSS): $(TGTFILE)
#	@$(ECHO_CMD) $(MSG_EXTENDED_LISTING)
#	$(VERBOSE_CMD)$(OBJDUMP) -h -S $< > $@
#	$(VERBOSE_NL)
$(LSS): $(TGTFILE)
	@$(ECHO_CMD) $(MSG_EXTENDED_LISTING)
	$(VERBOSE_CMD)$(OBJDUMP) -S $< > $@
	$(VERBOSE_NL)

# Create symbol table from target output file.
$(SYM): $(TGTFILE)
	@$(ECHO_CMD) $(MSG_SYMBOL_TABLE)
	$(VERBOSE_CMD)$(NM) -n $< > $@
	$(VERBOSE_NL)

ifeq ($(TGTTYPE),.elf)

# Create Intel HEX image from ELF output file.
$(HEX): $(TGTFILE)
	@$(ECHO_CMD) $(MSG_IHEX_IMAGE)
	$(VERBOSE_CMD)$(OBJCOPY) -O ihex $< $@
	$(VERBOSE_NL)

# Create binary image from ELF output file.
$(BIN): $(TGTFILE)
	@$(ECHO_CMD) $(MSG_BINARY_IMAGE)
	$(VERBOSE_CMD)$(OBJCOPY) -O binary $< $@
	$(VERBOSE_NL)

endif
