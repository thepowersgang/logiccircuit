#
#

CPPFLAGS = -I ./include/
CFLAGS = -Wall -g -std=gnu99

OBJ := main.o parse.o build.o
OBJ += Elements.Logic.o Element.NOT.o
OBJ += Element.CLOCK.o Element.COUNTER.o Element.MUX.o Element.DEMUX.o Element.LATCH.o
OBJ += Element.PULSE.o Element.HOLD.o Element.VALUESET.o Element.CONST.o Element.DELAY.o
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

