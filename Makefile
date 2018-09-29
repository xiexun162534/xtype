CLIENT_O = interface.o stopwatch.o protocol.o game.o error.o io_tools.o
SERVER_O = protocol.o error.o io_tools.o server.o
CC = gcc
CFLAGS = -Wall -ggdb3

xtype_game: xtype xtype_server

xtype: $(CLIENT_O)
	$(CC) $^ $(CFLAGS) -o $@

xtype_server: $(SERVER_O)
	$(CC) $^ $(CFLAGS) -o $@
