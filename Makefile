#
# CMPSC311 - Fall 2017
# Assignment #4 Makefile
#

# Variables
CC=gcc 
LINK=gcc
CFLAGS=-c -Wall -I. -fpic -g
LINKFLAGS=-L. -g
LINKLIBS=-lcrud -lgcrypt

# Files to build

HDD_CLIENT_OBJFILES=   hdd_sim.o \
                        hdd_file_io.o  \
                        hdd_client.o \
                    
TARGETS=    hdd_client
             
                    
# Suffix rules
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS)  -o $@ $<

# Productions

all : $(TARGETS) 
    
hdd_client: $(HDD_CLIENT_OBJFILES)
	$(LINK) $(LINKFLAGS) -o $@ $(HDD_CLIENT_OBJFILES) $(LINKLIBS) 

# Cleanup 
clean:
	rm -f $(TARGETS) $(HDD_CLIENT_OBJFILES)
