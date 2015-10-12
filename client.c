#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT_TEMP 8080
#define PORT_AIR 3000

int do_connect(int port);
float get_temperature();
void* monitoring_temperature();
bool air_conditioning(bool status);
void down_client();
void gotoxy(int x, int y);
void save_position();
void reset_position();
void* running_time();

volatile float temperature = 0;
static pthread_mutex_t mutexLock;
char *ip = "192.168.1.5";
int socket_descriptor_temp = 0;
int socket_descriptor_air = 0;


int main(int argc, char *argv[])
{
	signal(SIGINT, down_client);

	switch (argc)
	{
		case 2:
			ip = argv[1];
			break;
		case 1:
			break;
		default:
			errx(1, "Argumentos invalidos!!");
			break;
	}
	pthread_t time_thread;

	if(pthread_create(&time_thread, NULL, running_time, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	pthread_t temperature_thread;

	if(pthread_mutex_init(&mutexLock, NULL))
	{
		errx(1, "Erro ao criar o mutex");
	}

	if(pthread_create(&temperature_thread, NULL, monitoring_temperature, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	system("clear");

	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	gotoxy(w.ws_col / 2 - 12, 0);
	printf("Ar Condicionado: Desligado");
	
	printf("\n\nEscolha uma opcao: \n");
	printf("1 - Ligar o ar condicionado\n");
	printf("2 - Desligar o ar condicionado\n");
	printf("3 - Sair\n");
	printf("->");

	while(1)
	{
		int option;
		scanf("%d", &option);

		switch(option)
		{
			case 1:
				if(air_conditioning(true))
				{
					printf("O ar condicionado foi ligado com sucesso, pressione Enter");
					
					ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
					
					save_position();
					
					gotoxy(w.ws_col / 2 + 5, 0);
					
					printf("Ligado   ");

					reset_position();
				}

				else
					printf("Erro ao ligar o ar condicionado, pressione Enter para tentar novamente");

				getchar();
				getchar();
				break;
			case 2:
				if(air_conditioning(false))
				{
					printf("O ar condicionado foi desligado com sucesso, pressione Enter");

					ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
					
					save_position();
					
					gotoxy(w.ws_col / 2 + 5, 0);
					
					printf("Desligado");

					reset_position();
				}
				else
					printf("Erro ao desligar o ar condicionado, precione Enter para tente novamente");
				getchar();
				getchar();
				break;
			case 3:
				down_client();
				break;
			default:
				printf("Opção inválida");
				getchar();
				getchar();
		}
		gotoxy(0,8);
		printf("                                                                                       ");
		gotoxy(0,7);
		printf("-> ");
		gotoxy(3,7);
	}

	pthread_join(temperature_thread, NULL);
	pthread_mutex_destroy(&mutexLock);

	return 0;
}

void down_client()
{
	printf("\nDerrubando o cliente...\n");
	if (socket_descriptor_temp)
		close(socket_descriptor_temp);
	if (socket_descriptor_air)
		close(socket_descriptor_air);

	pthread_mutex_destroy(&mutexLock);

	exit(0);
}

int do_connect(int port)
{
	struct sockaddr_in addr_struct;
	int socket_descriptor;

	if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{
		errx(1, "Erro ao criar o socket");
	}

	addr_struct.sin_family = AF_INET;
	addr_struct.sin_port = htons(port);
	addr_struct.sin_addr.s_addr = inet_addr(ip);
	bzero(&(addr_struct.sin_zero), 8);

	if (connect(socket_descriptor,(struct sockaddr *)&addr_struct, sizeof(struct sockaddr)) ==-1) 
	{
		close(socket_descriptor);
		errx(1, "Erro ao conectar com o servidor");
	}

	return socket_descriptor;
}

float get_temperature()
{
	char *mensage = "get_temperature";
	int length = strlen(mensage) + 1;

	if (send(socket_descriptor_temp, &length, sizeof(length), 0) == -1)
	{
		close(socket_descriptor_temp);
		errx(1, "Erro ao enviar mensagem ao servidor");
	}

	if (send(socket_descriptor_temp, mensage, length, 0) == -1)
	{
		close(socket_descriptor_temp);
		errx(1, "Erro ao enviar mensagem ao servidor");
	}


	float temp;
	if (recv(socket_descriptor_temp, &temp, sizeof(temp), 0) == -1) 
	{
		close(socket_descriptor_temp);
		errx(1, "Erro ao receber mensagem");
	}

	return temp;
}

void* monitoring_temperature()
{
	while(1)
	{
		socket_descriptor_temp = do_connect(PORT_TEMP);
		float temp = get_temperature();

		pthread_mutex_lock(&mutexLock);
			temperature = temp;
		pthread_mutex_unlock(&mutexLock);

		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // retorna em w o tamanho da janela do terminal

		save_position();

		gotoxy(w.ws_col - 23, 0);

		printf("Temperatura atual: %.2f\n", temperature);

		reset_position();

		sleep(2);
	}
	
	close(socket_descriptor_temp);
}

bool air_conditioning(bool status)
{
	socket_descriptor_air = do_connect(PORT_AIR);

	char mensage[3];
	if(status)
		strcpy(mensage, "on");
	else
		strcpy(mensage, "of");

	int length = strlen(mensage) + 1;

	if (send(socket_descriptor_air, &length, sizeof(length), 0) == -1)
	{
		close(socket_descriptor_air);
		errx(1, "Erro ao enviar mensagem ao servidor");
	}

	if (send(socket_descriptor_air, mensage, length, 0) == -1)
	{
		close(socket_descriptor_air);
		errx(1, "Erro ao enviar mensagem ao servidor");
	}


	bool result;
	if (recv(socket_descriptor_air, &result, sizeof(result), 0) == -1) 
	{
		close(socket_descriptor_air);
		errx(1, "Erro ao receber mensagem");
	}
	
	return result;
}

void gotoxy(int x, int y)
{
	printf("\033[%d;%df", y, x);
	fflush(stdout);
}

void save_position()
{
	printf("\033[s");
	fflush(stdout);
}

void reset_position()
{
	printf("\033[u");
	fflush(stdout);
}

void* running_time()
{
	int i=0,j=0,k=0;
	for(i=0;i<24;k++)
	{
		save_position();

		gotoxy(0,0);

		printf("Tempo execucao: %02d:%02d:%02d",i,j,k);

		reset_position();
		sleep(1);

		if(k==59)
		{
			k=-1;
			if(j==59)
			{
				j=0;
				if(i==23)
					i=0;
				else
					i++;
			}
			else
				j++;
		}
	}
	return NULL;
}
