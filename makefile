CC := gcc

SRC := $(shell find . -name '*.c')
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

INC_DIRS := $(shell find lib -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS := -Wall -Wextra -std=c99 -O3
CPPFLAGS := $(INC_FLAGS) $(shell pkg-config --cflags jack) -MMD
LDLIBS := -ldl -lm $(shell pkg-config --libs jack)

all: main

debug: CFLAGS += -g -O0
debug: main

main: $(OBJ)

.PHONY: clean

# print-%  : ; @echo $* = $($*)
clean:
	$(RM) $(OBJ)	# remove object files
	$(RM) $(DEP)	# remove dependency files
	$(RM) main		# remove main program

-include $(DEP)

