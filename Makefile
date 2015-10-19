all:
	@echo "*** Compiling the server ***"
	@make server
	@echo "*** Compiling the client ***"
	@make client
	@echo "*** Done ***"

server:
	gcc -o server server.c -lpthread

client:
	gcc -o client client.c -lpthread

clear:
	rm -f client server *.log