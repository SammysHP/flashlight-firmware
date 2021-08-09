#==== Main Options =============================================================

MCU = attiny13
F_CPU = 4800000
#LFUSE = 0x75
#HFUSE = 0xFF

TARGET = rampinglight
SRC = $(TARGET).c

OBJDIR = obj
BACKUPDIR = backup



#==== Compile Options ==========================================================

CFLAGS = -mmcu=$(MCU)
CFLAGS += -I.
CFLAGS += -DF_CPU=$(F_CPU)UL
CFLAGS += -Os
#CFLAGS += -mint8
#CFLAGS += -mshort-calls
CFLAGS += -funsigned-char
CFLAGS += -funsigned-bitfields
CFLAGS += -fpack-struct
CFLAGS += -fshort-enums
#CFLAGS += -fno-unit-at-a-time
CFLAGS += -Wall
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wundef
#CFLAGS += -Wunreachable-code
#CFLAGS += -Wsign-compare
CFLAGS += -std=gnu99
#CFLAGS += -Wa,-adhlns=$(<:%.c=$(OBJDIR)/%.lst)
#CFLAGS += -flto
CFLAGS += -Wno-int-to-pointer-cast
#CFLAGS += -save-temps

#LDFLAGS =



#==== Programming Options (avrdude) ============================================

AVRDUDE_PROGRAMMER = stk500v1
AVRDUDE_PORT = /dev/ttyUSB0
AVRDUDE_BAUD = 19200

#AVRDUDE_NO_VERIFY = -V

AVRDUDE_FLAGS = -p $(MCU) -P $(AVRDUDE_PORT) -b $(AVRDUDE_BAUD) -c $(AVRDUDE_PROGRAMMER) $(AVRDUDE_NO_VERIFY)



#==== Targets ==================================================================

CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
AVRDUDE = avrdude
AVRSIZE = avr-size
REMOVE = rm -f
REMOVEDIR = rm -rf
TAIL = tail
AWK = awk

OBJ = $(SRC:%.c=$(OBJDIR)/%.o)

AVRDUDE_WRITE_FLASH = -U flash:w:$(TARGET).hex
AVRDUDE_WRITE_EEPROM = -U eeprom:w:$(TARGET).eep

MEMORYTYPES = flash eeprom fuse lfuse hfuse efuse boot calibration lock signature application apptable prodsig usersig


all: build


help:
	@echo 'Basic targets:'
	@echo '  build       Create all files.'
	@echo '  clean       Remove files created by make.'
	@echo '  size        Show the size of each section in the .elf file.'
	@echo
	@echo 'Create files:'
	@echo '  elf         Create binary .elf file.'
	@echo '  hex         Create .hex file containing .text and .data sections.'
	@echo '  eep         Create .eep file with the EEPROM content.'
	@echo '  lss         Create .lss file with a listing of the program.'
	@echo
	@echo 'Flashing:'
	@echo '  program     Write flash and EEPROM.'
	@echo '  flash       Write only flash.'
	@echo '  eeprom      Write only EEPROM.'
	@echo '  backup      Backup MCU content to "$(BACKUPDIR)". Available memory types:'
	@echo '              $(MEMORYTYPES)'
	@echo
	@echo 'Fuses:'
	@echo '  readfuses   Read fuses from MCU.'
	@echo '  writefuses  Write fuses to MCU using .fuse section.'
	@echo '  printfuses  Print fuses from .fuse section.'


build: elf hex eep lss size


elf: $(TARGET).elf
hex: $(TARGET).hex
eep: $(TARGET).eep
lss: $(TARGET).lss


program: flash eeprom


flash: $(TARGET).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_FLASH)


eeprom: $(TARGET).eep
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_EEPROM)


readfuses:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:r:-:h -U hfuse:r:-:h


#writefuses:
#	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m


writefuses: FUSES = $(shell $(OBJDUMP) -s --section=.fuse $(TARGET).elf | tail -1 | awk '{print substr($$2,1,2),substr($$2,3,2),substr($$2,5,2)}')
writefuses: $(TARGET).elf
	$(AVRDUDE) $(AVRDUDE_FLAGS) \
	$(if $(word 1,$(FUSES)),-U lfuse:w:0x$(word 1,$(FUSES)):m) \
	$(if $(word 2,$(FUSES)),-U hfuse:w:0x$(word 2,$(FUSES)):m) \
	$(if $(word 3,$(FUSES)),-U efuse:w:0x$(word 3,$(FUSES)):m)


printfuses: FUSES = $(shell $(OBJDUMP) -s --section=.fuse $(TARGET).elf | tail -1 | awk '{printf "l:0x%s h:0x%s e:0x%s",substr($$2,1,2),substr($$2,3,2),substr($$2,5,2)}')
printfuses: $(TARGET).elf
	@echo '$(FUSES)'


%.hex: %.elf
	$(OBJCOPY) -O ihex -j .text -j .data $< $@


%.eep: %.elf
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex $< $@


%.lss: %.elf
	$(OBJDUMP) -h -S $< > $@


.SECONDARY: $(TARGET).elf
.PRECIOUS: $(OBJ)
%.elf: $(OBJ)
	$(CC) $(CFLAGS) $^ --output $@ $(LDFLAGS)


$(OBJDIR)/%.o: %.c
	$(shell mkdir -p $(OBJDIR) 2>/dev/null)
	$(CC) -c $(CFLAGS) $< -o $@


size: $(TARGET).elf
	$(AVRSIZE) -A $(TARGET).elf
	$(AVRSIZE) -C --mcu=$(MCU) $(TARGET).elf


clean:
	$(REMOVE) "$(TARGET).hex"
	$(REMOVE) "$(TARGET).eep"
	$(REMOVE) "$(TARGET).elf"
	$(REMOVE) "$(TARGET).lss"
	$(REMOVE) "$(TARGET).i"
	$(REMOVE) "$(TARGET).s"
	$(REMOVEDIR) "$(OBJDIR)"


backup:
	$(shell mkdir -p $(BACKUPDIR) 2>/dev/null)
	@for memory in $(MEMORYTYPES); do \
	    $(AVRDUDE) $(AVRDUDE_FLAGS) -U $$memory:r:$(BACKUPDIR)/$(MCU).$$memory.hex:i; \
	done


.PHONY: all size build elf hex eep lss clean program flash eeprom readfuses writefuses printfuses backup help
