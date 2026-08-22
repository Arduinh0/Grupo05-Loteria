/*Código base obtido no site: https://www.geeksforgeeks.org/computer-networks/simple-client-server-application-in-c/*/

#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <time.h>       // Biblioteca para o horário de conexão
#include <pthread.h>    // API POSIX de Threads
#include <string.h>     // Operações com strings
#include <unistd.h>     // Chamadas de sistema UNIX como close() e sleep()

// Thread 1: Fica em loop aguardando mensagens enviadas pelo cliente
void* receber_dados_cliente(void* arg) {
    int clientSocket = *(int*)arg;
    char buffer[255];
    int bytes_recebidos;

    while (1) {
        // Aguarda recebimento de mensagens do cliente
        bytes_recebidos = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        // Verifica se a conexão com o cliente foi encerrada (retorno 0) ou se houve erro (retorno -1)
        if (bytes_recebidos <= 0) {
            printf("\nCliente desconectado ou ocorreu um erro na rede.\n");
            close(clientSocket);
            break; // Sai do loop para finalizar a thread de recebimento
        }
        
        // Adiciona terminador nulo na string recebida para exibir corretamente
        buffer[bytes_recebidos] = '\0';
        
        // Imprime a mensagem no terminal do servidor (apenas para efeitos de debug)
        printf("[Debug] Mensagem recebida do cliente: %s\n", buffer);
    }
    return NULL;
}

// Thread 2: Temporizador do sorteio que aguarda 60 segundos antes de avisar
void* temporizador_sorteio(void* arg) {
    int clientSocket = *(int*)arg;
    char msg_sorteio[] = "Sorteio realizado!";

    while (1) {
        sleep(60); // Aguarda 60 segundos
        
        // Envia a mensagem ao cliente de forma assíncrona
        send(clientSocket, msg_sorteio, strlen(msg_sorteio), 0);
        printf("[Sorteio] Mensagem de sorteio enviada ao cliente com sucesso.\n");
    }
    return NULL;
}

int main(int argc, char const* argv[])
{
    // Cria socket do servidor semelhante ao que foi feito no cliente
    int servSockD = socket(AF_INET, SOCK_STREAM, 0);

    // Define o endereco do servidor
    struct sockaddr_in servAddr;

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(9001);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    // Faz o bind do socket com o IP e porta especificados
    bind(servSockD, (struct sockaddr*)&servAddr, sizeof(servAddr));

    // Ouve por conexões
    listen(servSockD, 1);

    printf("Servidor da Loteria iniciado! Aguardando por conexão (1 cliente)...\n");
    
    // Aceita conexão e armazena o socket do cliente
    int clientSocket = accept(servSockD, NULL, NULL);
    printf("Um cliente foi conectado!\n");

    time_t t = time(NULL); 
    
    // Converte os segundos para a hora local da sua máquina
    struct tm *tm_info = localtime(&t);

    char serMsg[255]; // Variável unificada, redeclaração anterior removida

    // Formata a string usando o sprintf com os dados da struct tm
    sprintf(serMsg, "%02d:%02d:%02d: CONECTADO!!", 
            tm_info->tm_hour, 
            tm_info->tm_min, 
            tm_info->tm_sec);

    // Envia a mensagem com horário e confirmação para o socket do cliente
    send(clientSocket, serMsg, strlen(serMsg), 0);

    // Variáveis que vão armazenar os identificadores das threads
    pthread_t thread_recebimento, thread_sorteio;
    
    // Criando a Thread 1 (recebimento de mensagens)
    pthread_create(&thread_recebimento, NULL, receber_dados_cliente, (void*)&clientSocket);
    
    // Criando a Thread 2 (temporizador de sorteio)
    pthread_create(&thread_sorteio, NULL, temporizador_sorteio, (void*)&clientSocket);

    // O main aguarda pelo termino das threads
    pthread_join(thread_recebimento, NULL);
    pthread_join(thread_sorteio, NULL);

    close(servSockD);
    return 0;
}