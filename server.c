#include <err.h>
#include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

#define PORT_TEMP 8080
#define PORT_AIR 3000

void down_server();
int do_connect(int port);
void* receive_request_temp();
void* receive_request_air();

char *ip = "192.168.1.5";
int descriptor_server_temp = 0;
int descriptor_server_air = 0;
int descriptor_client_temp = 0;
int descriptor_client_air = 0;

main(int argc, char *argv[])
{
	signal(SIGINT, down_server);

	switch (argc)
	{
		case 2:
			ip = argv[1];
			break;
		case 1:
			break;
		default:
			perror("Argumentos invalidos!!");
			break;
	}

	pthread_t temperature_thread;
	pthread_t air_thread;

	descriptor_server_temp = do_connect(PORT_TEMP);
	descriptor_server_air = do_connect(PORT_AIR);

	if(pthread_create(&temperature_thread, NULL, receive_request_temp, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	if(pthread_create(&temperature_thread, NULL, receive_request_air, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	pthread_join(temperature_thread, NULL);
	pthread_join(air_thread, NULL);


	unlink(ip);	
	close(descriptor_server_temp);
	close(descriptor_server_air);
}

void down_server()
{
	printf("\nDerrubando o servidor...\n");
	if(descriptor_server_temp)
		close(descriptor_server_temp);
	if(descriptor_server_air)
		close(descriptor_server_air);
	if(descriptor_client_temp)
		close(descriptor_client_temp);
	if(descriptor_client_air)
		close(descriptor_client_air);
	unlink(ip);

	exit(0);
}

int do_connect(int port)
{
	struct sockaddr_in addr_server;
	int descriptor_server;
	if ((descriptor_server = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{
		errx(1, "Erro ao criar o socket");
	}

	addr_server.sin_family = AF_INET;
	addr_server.sin_port = htons(port);
	addr_server.sin_addr.s_addr = inet_addr(ip);
	bzero(&(addr_server.sin_zero), 8);

	if (bind(descriptor_server, (struct sockaddr *)&addr_server, sizeof(struct sockaddr))== -1) 
	{
		close(descriptor_server);
		errx(1, "Erro ao executar o bind");
	}
	if (listen(descriptor_server, 20) < 0) 
	{
		close(descriptor_server);
		errx(1, "Erro ao executar o linten");
	}

	return descriptor_server;
}

void* receive_request_temp()
{
	struct sockaddr_in addr_client;

	while(1)
	{
		int lenght = sizeof(struct sockaddr_in);
		descriptor_client_temp = accept(descriptor_server_temp, (struct sockaddr *)&addr_client,&lenght);
		
		if (descriptor_client_temp < 0)
		{
			perror("Erro ao executar o accept");
			continue;
		}

		int length;
		char* text;

		if(recv(descriptor_client_temp, &length, sizeof(length), 0) == 0)
		{
			perror("Erro na leitura");
			continue;
		}
		
		text = (char*) malloc(length);
		if(recv(descriptor_client_temp, text, length, 0) == 0)
		{
			perror("Erro na leitura");
			continue;
		}

		if(strcmp("get_temperature", text) == 0)
		{
			printf("Servidor: requisicao de temperatura recebida!\n");
			
			// float temperature = get_temp_uart();
			float temperature = 22.55;
			if (send(descriptor_client_temp, &temperature, sizeof(temperature), 0) == -1)
			{
				close(descriptor_client_temp);
				perror("Erro ao enviar resposta");
				continue;
			}
		}
		free(text);
		close(descriptor_client_temp);
	}
}

float get_temp_uart()
{
	int uart_descriptor;
	// Open the Port. We want read/write, no "controlling tty" status, and open it no matter what state DCD is in
	uart_descriptor = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
	if (uart_descriptor == -1)
	{
	  perror("Erro ao abrir o arquivo /dev/ttyAMA0");
	  return -273.15;
	}

	// Turn off blocking for reads, use (uart_descriptor, F_SETFL, FNDELAY) if you want that
	fcntl(uart_descriptor, F_SETFL, 0);

	//----- TX BYTES -----
	unsigned char buffer[3];
	unsigned char *p_buffer;
	
	p_buffer = &buffer[0];
	*p_buffer++ = 0x05;

	// Write to the port
	//int n = write(uart_descriptor, "Hello Peripheral\n", 17);
	int n = write(uart_descriptor, &buffer[0], (p_buffer - &buffer[0]));
	if (n < 0)
	{
		close(uart_descriptor);
		perror("Erro ao escrever na uart");
		return -273.15;
	}

	sleep(1);

	// Read up to 4 bytes from the port if they are there
	float temp;

	char buffer_temp[4];

	int m = read(uart_descriptor, (void*)buffer_temp, 4);

	memcpy(&temp, buffer_temp, 4);

	if (m < 0)
	{
	  perror("Erro ao ler a temperatura");
	  temp = -273.15;
	}
	else if (m == 0)
	{
		printf("Sem dados na porta\n");
		temp = -273.15;
	}

	close(uart_descriptor);
	return temp;
}

void* receive_request_air()
{
	struct sockaddr_in addr_client;

	while(1)
	{
		int lenght = sizeof(struct sockaddr_in);
		descriptor_client_air = accept(descriptor_server_air, (struct sockaddr *)&addr_client,&lenght);
		
		if (descriptor_client_air < 0)
		{
			perror("Erro ao executar o accept");
			continue;
		}

		int length;
		char* text;

		if(recv(descriptor_client_air, &length, sizeof(length), 0) == 0)
		{
			perror("Erro na leitura");
			continue;
		}
		
		text = (char*) malloc(length);
		if(recv(descriptor_client_air, text, length, 0) == 0)
		{
			perror("Erro na leitura");
			continue;
		}

		if(strcmp("on", text) == 0)
		{
			printf("Servidor: requisicao de ar condicionado recebida!\n");
			
			bool result = true;
			if (send(descriptor_client_air, &result, sizeof(result), 0) == -1)
			{
				close(descriptor_client_air);
				perror("Erro ao enviar resposta");
				continue;
			}
		}
		else if(strcmp("of", text) == 0)
		{
			printf("Servidor: requisicao de ar condicionado recebida!\n");
			
			bool result = false;
			if (send(descriptor_client_air, &result, sizeof(result), 0) == -1)
			{
				close(descriptor_client_air);
				perror("Erro ao enviar resposta");
				continue;
			}
		}
		free(text);
		close(descriptor_client_air);
	}
}
