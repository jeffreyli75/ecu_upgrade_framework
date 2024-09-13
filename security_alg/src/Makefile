CC = aarch64-linux-gnu-g++
# CC = g++
# CC = x86_64-w64-mingw32-c++-posix
# CC = arm-linux-gnueabihf-g++

export CC

INCLUDES =
INCLUDES += -I${PWD}
INCLUDES += -I../include


CFLAGS :=
CFLAGS += -g -fPIC -std=c++14
ifeq ($(CC),x86_64-w64-mingw32-c++-posix)
CFLAGS += -fpermissive
TARGET  := libsecurity_alg.dll
else
TARGET  := libsecurity_alg.so
CFLAGS += -DLINUX_OS
endif
export CFLAGS

LDFLAGS := -pthread -lstdc++
ifeq ($(CC),x86_64-w64-mingw32-c++-posix)
LDFLAGS +=  -lws2_32
endif

obj-c =
obj-c     += $(shell find $(SRCDIR) -name "*.cpp")
obj-c     += $(shell find $(SRCDIR) -name "*.c")
obj-o = 
obj-o += $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(obj-c)))

DEPENDENCY_LIST = $(patsubst %.o,%.d,$(obj-o))

all: $(obj-o)
	@echo "building taget "${TARGET}  
	${CC} -shared -o ${TARGET} $(obj-o) ${CFLAGS} ${LDFLAGS}

 

libs:
	@for dir in ${LIB_DIR}; do ${MAKE} -C $$dir; echo ""; done

.SUFFIXES: .cpp .o
.cpp.o:
	@echo "building file "$@    
	@${CC}   $(INCLUDES) ${CFLAGS} -w -c $< -MMD -MT $@ -o $@ ${LDFLAGS}

-include $(DEPENDENCY_LIST)


clean:
	rm *.o *.d
	rm ${TARGET}
    
.PHONY: all clean
