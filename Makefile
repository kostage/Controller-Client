CC=gcc
CFLAGS=-O0 -g3 -ggdb -Wall -DDEBUG -c -fmessage-length=0
SOURCE_ROOT= .
LDFLAGS=

# rsubdirs1 rsubdirs2 rsubdirs3 rwildcard
include $(SOURCE_ROOT)/Make.defines.func

SOURCE_DIRS = $(SOURCE_ROOT)

SOURCE_DIRS += $(call rsubdirs2, $(SOURCE_ROOT))

INCLUDE_DIRS := $(patsubst %, -I%, $(SOURCE_DIRS))

# pass include dirs to gcc via environment to not bloat output with -Idirectives
export C_INCLUDE_PATH:=$(subst $(space),$(colon),$(realpath $(INCLUDE_DIRS)))

CSOURCES := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)*.c))
COBJECTS=$(CSOURCES:.c=.o)

EXECUTABLE=module

all: $(CSOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(COBJECTS)
	$(CC) $(COBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

PHONY: clean debug

clean:
	rm -rf ./*.o ./$(EXECUTABLE) $(COBJECTS) $(COBJECTS)
