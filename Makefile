CC = gcc

all : waltsara.buildrooms.c waltsara.adventure.c
	$(CC) -o waltsara.buildrooms waltsara.buildrooms.c
	$(CC) -o waltsara.adventure waltsara.adventure.c -lpthread
