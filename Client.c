/*Código base obtido no site: https://www.geeksforgeeks.org/computer-networks/simple-client-server-application-in-c/*/

#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <pthread.h>    // API POSIX de Threads
#include <string.h>     // Operações com strings
#include <unistd.h>     // Chamadas de sistema UNIX como close() e sleep()
#include <signal.h>     // Tratamento de sinais (Ctrl+C)

// Variável global do socket para permitir o fechamento no tratador de sinal
int sockD = -1;

// Função para tratar o Ctrl+C (Encerramento Gracioso via Sinal)
void handle_sigint(int sig) {
    // Usamos write em vez de printf dentro de um signal handler por segurança (async-signal-safe)
    char msg[] = "\n[Sinal] Capturado SIGINT (Ctrl+C). Encerrando o cliente forçadamente...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    
    // 1 e 2. Fecha o socket que agora é global, garantindo que o servidor perceba a queda
    if (sockD != -1) {
        close(sockD);
    }
    
    // A thread do 'fgets' retém um 'lock' no stdin (I/O bloqueante). 
    // Chamar exit(0) tenta fazer flush nos buffers do stdio e causa DEADLOCK!
    // Por isso o programa congelava. Usamos _exit(0) para finalizar o processo de fato e imediatamente.
    _exit(0);
}

// Thread 1: Fica em loop lendo do teclado e enviando mensagens ao servidor
void* enviar_mensagem(void* arg) {
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
    char buffer[2048]; // Aumentado para suportar o boletim completo do servidor
    int bytes_recebidos;

    while (1) {
        // Aguarda recebimento de mensagens
        bytes_recebidos = recv(sockD, buffer, sizeof(buffer) - 1, 0);
        
        // Verifica se a conexão com o servidor foi encerrada (retorno 0) ou se houve erro (retorno -1)
        if (bytes_recebidos <= 0) {
            printf("\nServidor desconectado. Encerrando o cliente...\n");
            close(sockD);
            // O uso de exit(0) é mantido aqui porque a Thread 1 ficará bloqueada eternamente 
            // no fgets() (I/O bloqueante) aguardando o usuário digitar algo. 
            // Para interromper o programa, chamamos exit().
            exit(0);
        }
        
        // Finaliza a string de forma segura
        buffer[bytes_recebidos] = '\0';
        printf("Mensagem: %s\n", buffer);
    }
    return NULL;
}

int main(int argc, char const* argv[])
{
    // Registra o tratador do sinal SIGINT
    signal(SIGINT, handle_sigint);

    sockD = socket(AF_INET, SOCK_STREAM, 0);

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
        pthread_create(&thread_envio, NULL, enviar_mensagem, NULL);
        
        // Criando a Thread 2 (recebimento de dados)
        pthread_create(&thread_recebimento, NULL, receber_mensagem, NULL);
        
        // O main usa pthread_join para aguardar a execução das threads, 
        // evitando que o programa termine prematuramente.
        pthread_join(thread_envio, NULL);
        pthread_join(thread_recebimento, NULL);
    }

    printf("Programa cliente finalizado.\n");
    return 0;
}