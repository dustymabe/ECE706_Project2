CC = g++
OPT = -O0
OPT = -g
WARN = -w #-Wall
CFLAGS = $(OPT) $(WARN) $(INC) $(LIB)

# List all your .c files here (source files, excluding header files)
SIM_SRC = BitVector.cc Cache.cc CCSM.cc Dir.cc Net.cc
SIM_SRC+= simulator.cc Tile.cc

# List corresponding compiled object files here (.o files)
SIM_OBJ = BitVector.o Cache.o CCSM.o Dir.o Net.o
SIM_OBJ+= simulator.o Tile.o
 
#################################

# default rule

all: sim
	@echo "my work is done here..."


# rule for making sim

sim: $(SIM_OBJ)
	$(CC) -o sim $(CFLAGS) $(SIM_SRC) 
	@echo "-----------DONE WITH SIMULATOR-----------"


# generic rule for converting any .cc file to any .o file
 
.cc.o:
	$(CC) $(CFLAGS)  -c $*.cc


# type "make clean" to remove all .o files plus the sim_cache binary

clean:
	rm -f *.o sim


# type "make clobber" to remove all .o files (leaves sim_cache binary)

clobber:
	rm -f *.o


