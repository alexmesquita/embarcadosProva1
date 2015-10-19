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

#define PORT_TEMP 9000
#define PORT_AIR 7000

int do_connect(int port);
float get_temperature();
void* print_temperature();
bool air_conditioning(bool status);
void down_client();
void gotoxy(int x, int y);
void save_position();
void reset_position();
void* running_time();
void print_menu();

volatile float temperature = 0;
static pthread_mutex_t mutexLock;
char *ip = "15.0.62.18";
int socket_descriptor_temp = 0;
int socket_descriptor_air = 0;


int main(int argc, char *argv[])
{
	// Trata o sinal de interrupcao
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

	// cria as threads de tempo de execucao e de requisicao de temperatura e o mutex para acessar a temperatura
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

	if(pthread_create(&temperature_thread, NULL, print_temperature, NULL))
	{
		errx(1, "Erro ao criar a thread");
	}

	print_menu();

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
					
					// salva a posicao atual do cursor
					save_position();
					
					// verifica a largura da tela
					int w = screen_size();
					gotoxy(w / 2 + 5, 0);
					
					printf("Ligado   ");

					// volta para a posicao do cursor salva
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

					// salva a posicao atual do cursor
					save_position();
					
					// verifica a largura da tela
					int w = screen_size();
					gotoxy(w / 2 + 5, 0);
					
					printf("Ligado   ");

					// volta para a posicao do cursor salva
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

/*
* Ao receber o sinal de interrupção essa função é chamada. Ela fecha os sockets abertos
* e destroi o mutex criado, saindo do programa em seguida
*/
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

/*
*	Habilida o socket a aceitar uma nova comunicacao
*	Return: Descritor do socket
*/
int do_connect(int port)
{
	struct sockaddr_in addr_struct;
	int socket_descriptor;

	// Cria um novo socket
	socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
	if ( socket_descriptor == -1)
	{
		errx(1, "Erro ao criar o socket");
	}

	// Define as propriedades da conexao
	addr_struct.sin_family = AF_INET;
	addr_struct.sin_port = htons(port);
	addr_struct.sin_addr.s_addr = inet_addr(ip);
	bzero(&(addr_struct.sin_zero), 8);

	// Cria uma conexao com as propriedades passadas
	if (connect(socket_descriptor,(struct sockaddr *) &addr_struct, sizeof(struct sockaddr)) == -1) 
	{
		close(socket_descriptor);
		errx(1, "Erro ao conectar com o servidor");
	}

	return socket_descriptor;
}

/*
* Faz uma requisição ao servidor sobre a temperatura atual
* Return: temperatura atual
*/
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

/*
* Imprime a temperatura atual em um intervalo de tempo especifico
*/
void* print_temperature()
{
	while(1)
	{
		// Se conecta ao servidor na porta especifica da temperatura
		socket_descriptor_temp = do_connect(PORT_TEMP);
		float temp = get_temperature();

		// realiza o controle para o acesso a temperatura 
		pthread_mutex_lock(&mutexLock);
			temperature = temp;
		pthread_mutex_unlock(&mutexLock);

		int w = screen_size();

		save_position();

		gotoxy(w - 23, 0);

		if(temperature > -273)
		{
			printf("Temperatura atual: %.2f\n", temperature);
		}
		else
		{
			printf("Temperatura atual: ERRO\n");	
		}
		reset_position();

		sleep(5);
	}
	
	close(socket_descriptor_temp);
}

/*
* Funcao responsavel por setar um estado ao arcondicionado(Ligar/Desligar)
*/
bool air_conditioning(bool status)
{
	// Faz a conexao com o servidor na porta especificada
	socket_descriptor_air = do_connect(PORT_AIR);

	char mensage[4];
	if(status)
		strcpy(mensage, "on");
	else
		strcpy(mensage, "off");

	int length = strlen(mensage) + 1;

	// Envia o tamanho da mensagem que sera enviada ao servidor
	if (send(socket_descriptor_air, &length, sizeof(length), 0) == -1)
	{
		close(socket_descriptor_air);
		errx(1, "Erro ao enviar mensagem ao servidor");
	}

	// Envia a mensagem ao servido
	if (send(socket_descriptor_air, mensage, length, 0) == -1)
	{
		close(socket_descriptor_air);
		errx(1, "Erro ao enviar mensagem ao servidor");
	}


	// Recebe a resposta do servidor
	bool result;
	if (recv(socket_descriptor_air, &result, sizeof(result), 0) == -1) 
	{
		close(socket_descriptor_air);
		errx(1, "Erro ao receber mensagem");
	}
	
	return result;
}

/*
* Funcao que posiciona o curso em uma dada coordenada
*/
void gotoxy(int x, int y)
{
	printf("\033[%d;%df", y, x);
	fflush(stdout);
}

/*
* Funcao salva a posicao atual do cursor
*/
void save_position()
{
	printf("\033[s");
	fflush(stdout);
}

/*
* Funcao que restaura o cursor para a posicao previamente salva
*/
void reset_position()
{
	printf("\033[u");
	fflush(stdout);
}

/*
* Funcao que imprime o tempo de execucao do programa
*/
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

/*
*	Funcao que imprime o menu
*/
void print_menu()
{
	system("clear");

	int w = screen_size();

	// seta a posicao do cursor
	gotoxy(w / 2 - 12, 0);
	printf("Ar Condicionado: Desligado");
	
	printf("\n\nEscolha uma opcao: \n");
	printf("1 - Ligar o ar condicionado\n");
	printf("2 - Desligar o ar condicionado\n");
	printf("3 - Sair\n");
	printf("->");
}

/*
* Função que retorna a largura da tela
*/
int screen_size()
{
	// Estrutura para armazenar o tamanho da janela do terminal
	struct winsize win;
	// Seta os valores de tamanho da tela na estrutura winsize
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);

	return win.ws_col;
}
