
CC			= gcc
CXX         = g++
TARGET		= redis_test
INCLUDE		= -I$(current_dir)/hiredis  -I. 
LIBS        = -L./ ./hiredis/libhiredis.a -lpthread

CFLAGS   	=  -g -Wall -fPIC -std=c++14
OFLAGS = -g  -fPIC -Wall

THIS_SRCFILES:=$(wildcard *.cpp)
THIS_OBJS:=$(patsubst %.cpp,%.o,$(THIS_SRCFILES))
OBJS := $(THIS_OBJS)

$(TARGET):$(OBJS)
	$(CXX) $(LIBS) $(OFLAGS) -o $@ $^ $(LIBS)
%.o: %.cpp
	$(CXX)  $(INCLUDE) $(CFLAGS)  -c -o $@ $<  
%.o: %.c
	$(CC)  $(CFLAGS)  -c -o $@ $<   
clean:
	rm -rf $(TARGET)
	rm -rf *.o
	rm -rf *~


		
