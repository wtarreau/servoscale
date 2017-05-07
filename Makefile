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

CFLAGS = -std=gnu99 $(OPTIM) -funsigned-char -funsigned-bitfields \
  -fpack-struct -fshort-enums -Wall -DF_CPU=$(F_CPU) -mmcu=$(MCU) \
  -Wstrict-prototypes

AVRDUDE := $(TOOLS)/avrdude -C$(TOOLS)/avrdude.conf -c $(PROG) -P $(PORT) -p $(MCU)

CC      = $(TOOLS)/avr/bin/avr-gcc
CXX     = $(TOOLS)/avr/bin/avr-c++
OBJCOPY = $(TOOLS)/avr/bin/avr-objcopy
OBJDUMP = $(TOOLS)/avr/bin/avr-objdump
SIZE    = $(TOOLS)/avr/bin/avr-size

all: $(TARGET).hex

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
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.hex *.elf *.o
