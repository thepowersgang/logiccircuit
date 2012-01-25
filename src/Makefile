#
#

CPPFLAGS = -I ./include/
CFLAGS = -Wall -g -std=c99

OBJ := main.o parse.o build.o
OBJ += Elements.Logic.o Element.NOT.o
OBJ += Element.CLOCK.o Element.COUNTER.o Element.MUX.o Element.DEMUX.o Element.LATCH.o
OBJ += Element.PULSE.o Element.HOLD.o Element.VALUESET.o Element.CONST.o Element.DELAY.o
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

