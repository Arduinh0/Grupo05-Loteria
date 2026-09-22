/*Código base obtido no site: https://www.geeksforgeeks.org/computer-networks/simple-client-server-application-in-c/*/

#include <netinet/in.h> // Estrutura para armazenar informação dos endereços
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // APIs de Socket
#include <sys/types.h>
#include <pthread.h>    // API POSIX de Threads
#include <string.h>     // Operações com strings
#include <unistd.h>     // Chamadas de sistema UNIX como close() e sleep()
#include <signal.h>     // Tratamento de sinais (Ctrl+C)

// Variável global do socket para permitir o fechamento no tratador de sinal
int sockD = -1;

// Função para tratar o encerramento forçado
void handle_sigint(int sig) {
    (void)sig; // Evita warning de unused parameter
    char msg[] = "\n[Sinal] Capturado SIGINT (Ctrl+C). Encerrando o cliente forçadamente...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    
    // Fecha o socket
    if (sockD != -1) {
        close(sockD);
    }

    _exit(0);
}

// Thread 1: Loop lendo do teclado e enviando mensagens ao servidor
void* enviar_mensagem(void* arg) {
    (void)arg; // Evita warning de unused parameter
    char buffer[255];

    while (1) {
        // Lê o input
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            // Remove a quebra de linha
            buffer[strcspn(buffer, "\n")] = '\0';
            
            // Envia a mensagem ao servidor
            int bytes_enviados = send(sockD, buffer, strlen(buffer), 0);
            if (bytes_enviados < 0) {
                printf("\n[Erro] Falha ao enviar a mensagem. Conexao com o servidor perdida.\n");
                close(sockD);
                exit(1);
            }
            
            // Verifica localmente se a mensagem enviada foi o comando de saída
            if (strncmp(buffer, ":quit", 5) == 0) {
                printf("\n[Sistema] Desconectando do servidor e encerrando o cliente...\n");
                close(sockD);
                exit(0);
            }
        }
    }
    return NULL;
}

// Thread 2: Loop aguardando e imprimindo mensagens vindas do servidor
void* receber_mensagem(void* arg) {
    (void)arg; // Evita warning de unused parameter
    char buffer[2048];
    int bytes_recebidos;

    while (1) {
        // Aguarda recebimento de mensagens
        bytes_recebidos = recv(sockD, buffer, sizeof(buffer) - 1, 0);
        
        // Verifica se a conexão com o servidor foi encerrada (0) ou se houve erro (-1)
        if (bytes_recebidos <= 0) {
            printf("\nServidor desconectado. Encerrando o cliente...\n");
            close(sockD);
            exit(0);
        }
        
        // Finaliza a string
        buffer[bytes_recebidos] = '\0';
        printf("%s\n", buffer);
    }
    return NULL;
}

int main(int argc, char const* argv[])
{
    // Evita warnings do compilador para variaveis nao utilizadas
    (void)argc;
    (void)argv;

    // Ignora o sinal SIGPIPE para evitar crash se o servidor cair durante um envio
    signal(SIGPIPE, SIG_IGN);

    // Registra o tratador do sinal SIGINT
    signal(SIGINT, handle_sigint);

    sockD = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servAddr;

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(9001);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    int connectStatus = connect(sockD, (struct sockaddr*)&servAddr, sizeof(servAddr));

    if (connectStatus == -1) {
        perror("\n[Erro] Falha ao conectar ao servidor");
        exit(1);
    }
    else {
        char strData[1024];

        // Aguarda receber a mensagem inicial
        int bytes = recv(sockD, strData, sizeof(strData) - 1, 0);
        if (bytes > 0) {
            strData[bytes] = '\0';
            printf("\n%s\n", strData);
        }

        // Identificadores das threads
        pthread_t thread_envio, thread_recebimento;
        
        // Envio de dados
        pthread_create(&thread_envio, NULL, enviar_mensagem, NULL);
        
        // Recebimento de Dados
        pthread_create(&thread_recebimento, NULL, receber_mensagem, NULL);
        
        // pthread_join para aguardar a execução das threads
        pthread_join(thread_envio, NULL);
        pthread_join(thread_recebimento, NULL);
    }

    printf("Programa cliente finalizado.\n");
    return 0;
}