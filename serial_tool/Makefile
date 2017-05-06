CC=gcc
CFLAGS= -ggdb -Wall -std=gnu99 $$(mysql_config --cflags --libs) -lpthread
EXEC=serial_tool
OBJS= *.c

all:
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)
	chmod +x $(EXEC)
install:
	sudo mv $(EXEC) /usr/local/bin/
