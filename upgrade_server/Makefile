CXX = g++
CC = gcc

CPPFLAGS = -g -Wall -Wextra -std=c++11 -fsanitize=address -DUDS_TP_MTU=65535 -DUDS_TP_ISOTP_SOCK=1 -D_Static_assert=static_assert
LDFLAGS = -g -Llib -ldoipcommon -ldoipserver -L. -luds -lpthread

CSRCS += src/iso14229.c
HDRS += iso14229.h
CFLAGS = -DUDS_TP_ISOTP_SOCK=1 -DUDS_TP_MTU=65535 -DUDS_LINES -g -Iinclude
COBJS := $(CSRCS:.c=.o)
LIB := libuds.so

SRCPATH = src
INCPATH = include
BUILDPATH = build

TARGET = upgrade_server

# List of all source files, separated by whitespace
SOURCE = $(wildcard $(SRCPATH)/*.cpp)


OBJS = $(patsubst $(SRCPATH)/%.cpp, $(BUILDPATH)/%.o, $(SOURCE))

.PHONY: all clean

all: env $(BUILDPATH)/$(TARGET)

env:
	mkdir -p $(BUILDPATH)

clean:
	rm -rf $(BUILDPATH)/* 
	rm $(LIB)
	rm $(COBJS)

$(BUILDPATH)/%.o: $(SRCPATH)/%.cpp
	$(CXX) $(CPPFLAGS) -I $(INCPATH) -fPIC -c $< -o $@
	
    
$(BUILDPATH)/$(TARGET): $(OBJS) $(LIB)
	$(CXX) $(CPPFLAGS) $^ $(LDFLAGS) -o $@
	
$(COBJS):%.o:%.c
	$(CC) $(CFLAGS) -c -fPIC $^ -o $@
$(LIB): $(COBJS)
	$(CC) $(CFLAGS) -shared -o -fPIC -o $@ $^

