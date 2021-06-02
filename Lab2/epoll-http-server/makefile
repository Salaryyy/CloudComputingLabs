src = $(wildcard ./*.c)  
obj = $(patsubst ./%.c, ./%.o, $(src)) 

myArgs= -Wall -g 
target=server
CC=gcc

ALL:$(target)

$(target):$(obj)
	$(CC) $^ -lpthread -o $@ $(myArgs) 

$(obj):%.o:%.c
	$(CC) -c $^ -o $@ $(myArgs)

clean:
	-rm -rf $(obj) $(target)

.PHONY: clean ALL

