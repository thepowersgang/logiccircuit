#
#

CPPFLAGS = -I ./include/
CFLAGS = -Wall -Werror -g

OBJ := main.o parse.o build.o
OBJ += Element.AND.o Element.OR.o Element.XOR.o Element.NOT.o
OBJ += Element.NAND.o Element.NOR.o Element.NXOR.o
OBJ += Element.CLOCK.o Element.COUNTER.o Element.DELAY.o Element.DEMUX.o
OBJ += Element.PULSE.o Element.HOLD.o Element.VALUESET.o
BIN = ../logic_cct

.PHONY: all clean

all: $(BIN)

clean:
	$(RM) $(BIN) $(OBJ)

$(BIN): $(OBJ)
	$(CC) -g -o $(BIN) $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS) $(CPPFLAGS)
#	$(CC) -M -MF $@ -o $*.d $< $(CPPFLAGS)

