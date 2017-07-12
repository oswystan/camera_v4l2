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
src := main.c camera.c
obj := $(patsubst %.c,%.o,$(src))
ld_flags := -ludev

all: $(bin)


$(bin): $(obj)
	@gcc $^ -o $(bin) $(ld_flags)
	@echo "GEN\t"$@

%.o:%.c
	@echo "gcc\t"$@
	@gcc -c $< -o $@

clean:
	@rm -f *.o *.yuv *.jpg $(bin)

#######################################################################
