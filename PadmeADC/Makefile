########################################################################
#
# Compile PadmeADC code for RasberryPi
#
# Based on the PadmeDAQ module
#
# Author Emanuele Leonardi <emanuele.leonardi@roma1.infn.it>
#
#########################################################################

EXE =	PadmeADC.exe

SDIR	= src
ODIR	= obj
IDIR	= include

SRC =	$(wildcard $(SDIR)/*.c)
INC =	$(wildcard $(IDIR)/*.h)

OBJ =	$(addprefix $(ODIR)/,$(notdir $(SRC:.c=.o)))

DEPS = $(INC) Makefile

CC	=	gcc
CFLAGS	=	-fPIC -DLINUX -O2 -g -Wall -I$(IDIR) -I$(CAENDIR)/include
# Use these for better debug
#CC	=	g++
#CFLAGS	=	-DLINUX -O0 -g -Wall -I$(IDIR) -I$(CAENDIR)/include

LIBS	=	-L$(CAENDIR)/lib -lCAENDigitizer -lm

#########################################################################

all:	$(EXE)

$(EXE):	$(OBJ)
	$(CC) $(FLAGS) -o $(EXE) $(OBJ) $(LIBS)

$(ODIR)/%.o:	$(SDIR)/%.c $(DEPS)
		$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:	clean cleanall try

clean:
	rm -f $(ODIR)/*.o

cleanall:
	rm -f $(EXE) $(ODIR)/*.o

try:
	@echo $(EXE)
	@echo $(SRC)
	@echo $(INC)
	@echo $(OBJ)
