all: client server

client: distcomclient.c
	gcc -o distcomclient distcomclient.c

#distcomclient.o: distcomclient.c
#	gcc -c distcomclient.c

server: distcomserver.c
	gcc -o distcomserver distcomserver.c -lpthread

#distcomserver.o: distcomserver.c
#	gcc -c distcomserver.c
#
#distcomclient.o: distcomclient.c
#	gcc -c distcomclient.c
#
#.PHONY: clean
#clean:
#	rm -f client distcomclient.o server distcomserver.o