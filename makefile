#######################################################################
##                     Copyright (C) 2017 wystan
##
##       filename: makefile
##    description:
##        created: 2017-07-06 11:42:13
##         author: wystan
##
#######################################################################
.PHONY: all clean

bin := camera
src := $(wildcard *.c *.cpp)
obj := $(patsubst %.c,%.o,$(src))
obj := $(patsubst %.cpp,%.o,$(obj))
ld_flags := -ludev

all: $(bin)


$(bin): $(obj)
	@gcc $^ -o $(bin) $(ld_flags)
	@echo "[gen]\t"$@

%.o:%.c
	@echo "[ cc]\t"$@
	@gcc -c $< -o $@
%.o:%.cpp
	@echo "[cpp]\t"$@
	@g++ -c $< -o $@

clean:
	@echo "cleaning..."
	@rm -f *.o *.yuv *.jpg $(bin)
	@echo "done."

#######################################################################
