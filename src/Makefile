#
#

CPPFLAGS = -I ./include/
CFLAGS = -Wall -g -std=gnu99
CFLAGS += -O3
LDFLAGS = -lm -lpthread

OBJ := main.o lex.o parse.o build.o compiled.o sim.o
OBJ += render.o
OBJ += Elements.Logic.o Element.NOT.o
OBJ += Element.CLOCK.o Element.COUNTER.o Element.MUX.o Element.DEMUX.o Element.LATCH.o
OBJ += Element.PULSE.o Element.HOLD.o Element.DELAY.o Element.ENABLE.o
OBJ += Element.SEQUENCER.o Element.MEMORY_DRAM.o Element.FILEROM.o
OBJ += Element.ADDER.o
BIN = ../logic_cct
OBJ := $(OBJ:%=obj/%)

.PHONY: all clean

all: $(BIN)

clean:
	$(RM) $(BIN) $(OBJ) $(OBJ:%.o=%.d)

$(BIN): $(OBJ)
	$(CC) -g -o $(BIN) $(OBJ) $(LDFLAGS)

obj/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) -o $@ -c $< $(CFLAGS) $(CPPFLAGS) -MMD -MP

-include $(OBJ:%.o=%.d)

