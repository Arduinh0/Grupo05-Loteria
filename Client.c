/*Código base obtido no site: https://www.geeksforgeeks.org/computer-networks/simple-client-server-application-in-c/*/

#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <pthread.h>    // API POSIX de Threads
#include <string.h>     // Operações com strings
#include <unistd.h>     // Chamadas de sistema UNIX como close() e sleep()

// Thread 1: Fica em loop lendo do teclado e enviando mensagens ao servidor
void* enviar_mensagem(void* arg) {
    int sockD = *(int*)arg;
    char buffer[255];

    while (1) {
        // Lê a entrada do usuário pelo terminal
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            // Remove a quebra de linha (\n) lida pelo fgets, caso exista
            buffer[strcspn(buffer, "\n")] = '\0';
            
            // Envia a mensagem ao servidor
            send(sockD, buffer, strlen(buffer), 0);
        }
    }
    return NULL;
}

// Thread 2: Fica em loop aguardando e imprimindo mensagens vindas do servidor
void* receber_mensagem(void* arg) {
    int sockD = *(int*)arg;
    char buffer[255];
    int bytes_recebidos;

    while (1) {
        // Aguarda recebimento de mensagens
        bytes_recebidos = recv(sockD, buffer, sizeof(buffer) - 1, 0);
        
        // Verifica se a conexão com o servidor foi encerrada (retorno 0) ou se houve erro (retorno -1)
        if (bytes_recebidos <= 0) {
            printf("\nServidor desconectado. Encerrando o cliente...\n");
            close(sockD);
            exit(0); // Encerra a aplicação do cliente com sucesso
        }
        
        // Finaliza a string de forma segura
        buffer[bytes_recebidos] = '\0';
        printf("Mensagem: %s\n", buffer);
    }
    return NULL;
}

int main(int argc, char const* argv[])
{
    int sockD = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servAddr;

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(9001); // use some unused port number
    servAddr.sin_addr.s_addr = INADDR_ANY;

    int connectStatus = connect(sockD, (struct sockaddr*)&servAddr, sizeof(servAddr));

    if (connectStatus == -1) {
        printf("Error...\n");
    }
    else {
        char strData[255];

        // Aguarda receber a mensagem inicial de "CONECTADO!!"
        int bytes = recv(sockD, strData, sizeof(strData) - 1, 0);
        if (bytes > 0) {
            strData[bytes] = '\0';
            printf("Message: %s\n", strData);
        }

        // Variáveis que vão armazenar os identificadores das threads
        pthread_t thread_envio, thread_recebimento;
        
        // Criando a Thread 1 (envio de dados)
        pthread_create(&thread_envio, NULL, enviar_mensagem, (void*)&sockD);
        
        // Criando a Thread 2 (recebimento de dados)
        pthread_create(&thread_recebimento, NULL, receber_mensagem, (void*)&sockD);
        
        // O main usa pthread_join para aguardar a execução das threads, 
        // evitando que o programa termine prematuramente.
        pthread_join(thread_envio, NULL);
        pthread_join(thread_recebimento, NULL);
    }

    return 0;
}