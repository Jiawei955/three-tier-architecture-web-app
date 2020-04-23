CC = gcc
CXX = g++

INCLUDES = 
CFLAGS = -g -Wall $(INCLUDES)
CXXFLAGS = -g -Wall $(INCLUDES)

.PHONY: default 
default: http-server

.PHONY: clean
clean:
	rm -f *.o *~ a.out http-server


