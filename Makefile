MCU    := attiny85
F_CPU  := 8000000
FUSE   := -U lfuse:w:0xc1:m -U hfuse:w:0xdd:m -U efuse:w:0xfe:m
#MCU   := attiny13
#F_CPU := 9600000
#FUSE  := -U lfuse:w:0x7a:m -U hfuse:w:0xff:m
TARGET := servoscale
PROG   := buspirate
PORT   := /dev/ttyUSB0
TOOLS  := $(HOME)/trinket-arduino-1.0.5/hardware/tools
OPTIM  := -Os

COMMONFLAGS = -g $(OPTIM) -funsigned-char -funsigned-bitfields \
  -fpack-struct -fshort-enums -Wall -DF_CPU=$(F_CPU) -mmcu=$(MCU)

CFLAGS = -std=gnu99 $(COMMONFLAGS) -Wstrict-prototypes \
  -Wa,-adhlns=$(<:.c=.lst) \

CXXFLAGS = -std=gnu++11 $(COMMONFLAGS) -fno-exceptions -fno-rtti \
  -Wa,-adhlns=$(<:.cc=.lst) \

ASFLAGS = -Wa,-adhlns=$(<:.S=.lst),-gstabs
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref

AVRDUDE := $(TOOLS)/avrdude -C$(TOOLS)/avrdude.conf -c $(PROG) -P $(PORT) -p $(MCU)

CC      = $(TOOLS)/avr/bin/avr-gcc
CXX     = $(TOOLS)/avr/bin/avr-c++
OBJCOPY = $(TOOLS)/avr/bin/avr-objcopy
OBJDUMP = $(TOOLS)/avr/bin/avr-objdump
SIZE    = $(TOOLS)/avr/bin/avr-size

default: $(TARGET).hex

check:
	$(AVRDUDE)

fuse:
	$(AVRDUDE) $(FUSE)

upload:
	$(AVRDUDE) -U flash:w:$(TARGET).hex

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(SIZE) $@

%.elf: %.o
	$(CC) $(CFLAGS) $< --output $@ $(LDFLAGS)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.s : %.c
	$(CC) -S $(CFLAGS) $< -o $@

%.o : %.S
	$(CC) -c $(ALL_ASFLAGS) $< -o $@

clean:
	-rm -f *.hex *.lst *.obj *.elf *.o *.map

.SECONDARY : $(TARGET).elf
.PRECIOUS : $(OBJ)
