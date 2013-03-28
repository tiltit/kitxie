PRG			= kitxie

MCU_TARGET		= attiny2313
SYSTEM_CLOCK		= 16000000UL

DEFS			=
LIBS			=

CC			= avr-gcc

OBJCOPY			= avr-objcopy
OBJDUMP			= avr-objdump

SOURCES=$(PRG).c
OBJECTS=$(SOURCES:.c=.o)

all: $(PRG).hex

#$(PRG).o: $(PRG).c
#	$(CC) -Os -DF_CPU=$(SYSTEM_CLOCK) -mmcu=$(MCU_TARGET) -c -o $(PRG).o $(PRG).c  $(LIBS)
.c.o:
	$(CC) -Os -DF_CPU=$(SYSTEM_CLOCK) -mmcu=$(MCU_TARGET) -c -o $@ $< $(LIBS)

$(PRG).elf: $(OBJECTS)
	$(CC) -mmcu=$(MCU_TARGET) $(OBJECTS) -o $(PRG).elf

$(PRG).hex: $(PRG).elf
	$(OBJCOPY) -O ihex -R .eeprom $(PRG).elf $(PRG).hex

$(PRG).lst: $(PRG).elf
	$(OBJDUMP) -h -S $(PRG).elf > $(PRG).lst

$(PRG).map: $(PRG).elf $(PRG).o
	$(CC) -g -mmcu=$(MCU_TARGET) -Wl,-Map,$(PRG).map -o $(PRG).elf $(PRG).o

flash: $(PRG).hex
	avrdude -F -V -c stk500v2  -p t2313 -P /dev/stk500-programmer0 -b 115200 -U flash:w:$(PRG).hex

clean:
	rm -rf *.o *.elf *.lst *.map *.hex

