CC = gcc

NAS_DIR = ..


rb_tool: rb_tool.c 
	$(CC) rb_tool.c -o rb_tool -I../../../mac/openair_wireless3g4free/openair_rrc/include -I../ -g -ggdb 

all:    rb_tool

clean:
	rm rb_tool
