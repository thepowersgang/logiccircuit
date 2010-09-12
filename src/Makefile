#
#

CPPFLAGS = -I ./include/


OBJ := main.o
OBJ += Element.AND.o Element.NAND.o
OBJ += Element.NOR.o Element.XOR.o
BIN = ../logic_cct

.PHONY: all clean

all: $(BIN)

clean:
	$(RM) $(BIN) $(OBJ)

$(BIN): $(OBJ)
	$(CXX) -o $(BIN) $(OBJ) $(LDFLAGS)

%.o: %.cc
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(CPPFLAGS)

