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
bool change_air_state(bool state);

char *ip = "192.168.1.5";
int descriptor_server_temp = 0;
int descriptor_server_air = 0;
int descriptor_client_temp = 0;
int descriptor_client_air = 0;

main(int argc, char *argv[])
{
	// Trata o sinal de interrupcao
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

	// Faz a conexao nas porta definidas
	descriptor_server_temp = do_connect(PORT_TEMP);
	descriptor_server_air = do_connect(PORT_AIR);

	// Cria as threads
	if(pthread_create(&temperature_thread, NULL, receive_request_temp, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	if(pthread_create(&air_thread, NULL, receive_request_air, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	// Espera as threads terminarem
	pthread_join(temperature_thread, NULL);
	pthread_join(air_thread, NULL);

	// Desassocia o socket ao ip e fecha as conexoes
	unlink(ip);	
	close(descriptor_server_temp);
	close(descriptor_server_air);
}

/*
*	Ao receber o sinal de interrupcao todas as conexoes sao encerradas e fecha o programa
*/
void down_server()
{
	printf("\nDerrubando o servidor...\n");
	if(descriptor_server_temp)
	{
		close(descriptor_server_temp);
	}
	if(descriptor_server_air)
	{
		close(descriptor_server_air);
	}
	if(descriptor_client_temp)
	{
		close(descriptor_client_temp);
	}
	if(descriptor_client_air)
	{
		close(descriptor_client_air);
	}
	unlink(ip);

	exit(0);
}

/*
*	Habilida o socket a aceitar uma nova comunicacao
*	Return: Descritor do socket
*/
int do_connect(int port)
{
	struct sockaddr_in addr_server;
	int descriptor_server;

	// Cria o socket
	if ((descriptor_server = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{
		errx(1, "Erro ao criar o socket");
	}

	// Define as propriedades da conexao
	addr_server.sin_family = AF_INET;
	addr_server.sin_port = htons(port);
	addr_server.sin_addr.s_addr = inet_addr(ip);
	bzero(&(addr_server.sin_zero), 8);

	// Associa uma porta ao socket
	if (bind(descriptor_server, (struct sockaddr *)&addr_server, sizeof(struct sockaddr))== -1) 
	{
		close(descriptor_server);
		errx(1, "Erro ao executar o bind");
	}
	// Habilita a porta para escuta
	if (listen(descriptor_server, 20) < 0) 
	{
		close(descriptor_server);
		errx(1, "Erro ao executar o linten");
	}

	return descriptor_server;
}
/*
*	Recebe uma requisicao de temperatura, chama a uart para conseguir a temperatura
*	atual e responde esta temperatura
*/
void* receive_request_temp()
{
	struct sockaddr_in addr_client;

	while(1)
	{
		int length = sizeof(struct sockaddr_in);
		descriptor_client_temp = accept(descriptor_server_temp, (struct sockaddr *)&addr_client,&length);
		
		if (descriptor_client_temp < 0)
		{
			perror("Erro ao executar o accept");
			continue;
		}

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

/*
*	Requisita a temperatura do uart a retorna
*/
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
	int n = write(uart_descriptor, &buffer[0], (p_buffer - &buffer[0]));
	if (n < 0)
	{
		close(uart_descriptor);
		perror("Erro ao escrever na uart");
		return -273.15;
	}

	sleep(1);


	char buffer_temp[4];

	// Lê 4 bytes da uart
	int m = read(uart_descriptor, (void*)buffer_temp, 4);

	float temp;
	// verifica se houve erra na leitura, caso tenha ocorrido é retornado -273.15 (Zero absoluto) 
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

	// seta na variavel do tipo float a temperatura
	memcpy(&temp, buffer_temp, 4);

	close(uart_descriptor);
	return temp;
}

/*
*	Recebe uma requisicao para ligar ou desligar o ar condicionado
*/
void* receive_request_air()
{
	struct sockaddr_in addr_client;

	while(1)
	{
		int length = sizeof(struct sockaddr_in);

		// Aguada um cliente se conectar ao socket que controla o ar condicionado
		descriptor_client_air = accept(descriptor_server_air, (struct sockaddr *)&addr_client,&length);
		
		if (descriptor_client_air < 0)
		{
			perror("Erro ao executar o accept");
			continue;
		}

		char* text;

		// Lê qual o tamanho da mensagem que será recebida
		if(recv(descriptor_client_air, &length, sizeof(length), 0) == 0)
		{
			perror("Erro na leitura");
			continue;
		}
		
		text = (char*) malloc(length);
		// Recebe a mensagem
		if(recv(descriptor_client_air, text, length, 0) == 0)
		{
			perror("Erro na leitura");
			continue;
		}

		bool result = false;
		// Verifica se a mensagem recebida é para ligar ou desligar o ar condicionado
		if(strcmp("on", text) == 0)
		{
			printf("Servidor: requisicao de ar condicionado recebida!\n");
			result = true;
		}
		else if(strcmp("off", text) == 0)
		{
			printf("Servidor: requisicao de ar condicionado recebida!\n");
			result = false;
		}

		// muda o estado do ar condicionado via uart
		// result = change_air_state(result);
		if (send(descriptor_client_air, &result, sizeof(result), 0) == -1)
		{
			close(descriptor_client_air);
			perror("Erro ao enviar resposta");
			continue;
		}

		free(text);
		close(descriptor_client_air);
	}
}
/*
*	Envia o novo estado para o ar condicionado
*	Return: True se a operacao foi executada com sucesso, E False, caso contrario
*/
bool change_air_state(bool state)
{
	int uart_descriptor;
	// Open the Port. We want read/write, no "controlling tty" status, and open it no matter what state DCD is in
	uart_descriptor = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
	if (uart_descriptor == -1)
	{
	  perror("Erro ao abrir o arquivo /dev/ttyAMA0");
	  return false;
	}

	// Turn off blocking for reads, use (uart_descriptor, F_SETFL, FNDELAY) if you want that
	fcntl(uart_descriptor, F_SETFL, 0);

	//----- TX BYTES -----
	unsigned char buffer[2];
	unsigned char *p_buffer;
	
	p_buffer = &buffer[0];
	*p_buffer++ = 0xA0;
	if(state)
	{
		*p_buffer++ = 0x01;
	}
	else
	{
		*p_buffer++ = 0x00;	
	}

	// Write to the port
	int n = write(uart_descriptor, &buffer[0], (p_buffer - &buffer[0]));
	if (n < 0)
	{
		close(uart_descriptor);
		perror("Erro ao escrever na uart");
		return false;
	}

	sleep(1);

	char buffer_result;

	int m = 0;
	// int m = read(uart_descriptor, (void*) &buffer_result, 1);

	bool result;
	if (m <= 0)
	{
		perror("Erro ao ler a resposta do ar condicionado");
		result = false;
	}

	if (buffer_result != 0xA1)
	{
		perror("O arduino recebeu um comando desconhecido");
		result = false;
	}
	else
	{
		result = true;
	}

	close(uart_descriptor);
	return result;
}
