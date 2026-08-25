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
#include <ctype.h>      // Biblioteca para validação de caracteres (isdigit)
#include <signal.h>     // Tratamento de sinais (Ctrl+C)

// Estados Globais da Loteria
#define MAX_APOSTAS 100
#define MAX_NUMS 20

typedef struct {
    int num[MAX_NUMS];
    int qtd_n;
} Aposta;

// Configuração Padrão da Loteria
int config_i = 0;
int config_f = 100;
int config_qtd = 5;

Aposta lista_apostas[MAX_APOSTAS];
int total_apostas = 0;

// Variáveis globais para grateful ending e Sinal
int cliente_conectado = 1;
int servSockD = -1;
int clientSocket = -1;

// Mutex para evitar concorrência de leitura e escrita
pthread_mutex_t m_loteria = PTHREAD_MUTEX_INITIALIZER;

// Tratador de sinal para grateful ending via Ctrl+C
void handle_sigint(int sig) {
    printf("\n[Sinal] Capturado SIGINT (Ctrl+C). Fechando sockets e encerrando...\n");
    if (clientSocket != -1) close(clientSocket);
    if (servSockD != -1) close(servSockD);
    exit(0);
}

// Thread 1: Fica em loop aguardando mensagens enviadas pelo cliente
void* receber_dados_cliente(void* arg) {
    char buffer[255];
    int bytes_recebidos;

    while (1) {
        bytes_recebidos = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recebidos <= 0) {
            printf("\n[Sistema] Cliente desconectado ou erro de rede.\n");
            
            // Flag protegida por mutex indicando que a conexao caiu
            pthread_mutex_lock(&m_loteria);
            cliente_conectado = 0;
            pthread_mutex_unlock(&m_loteria);
            
            break; // Sai do loop para a thread terminar
        }

        buffer[bytes_recebidos] = '\0';
        
        // Trava o Mutex antes de ler/alterar as globais
        pthread_mutex_lock(&m_loteria);

        // Verifica se a string recebida é um comando (inicia com ':')
        if (buffer[0] == ':') {
            int valor;
            if (sscanf(buffer, ":inicio %d", &valor) == 1) {
                config_i = valor;
                printf("[Config] Inicio alterado para: %d\n", config_i);
            } 
            else if (sscanf(buffer, ":fim %d", &valor) == 1) {
                config_f = valor;
                printf("[Config] Fim alterado para: %d\n", config_f);
            } 
            else if (sscanf(buffer, ":qtd %d", &valor) == 1) {
                config_qtd = valor;
                printf("[Config] Qtd de numeros sorteados alterada para: %d\n", config_qtd);
            }
        } 
        // Se não for comando, interpreta como uma aposta (números separados por espaço)
        else {
            if (total_apostas < MAX_APOSTAS) {
                Aposta nova_aposta;
                nova_aposta.qtd_n = 0;

                // Divide a string pelos espaços
                char* token = strtok(buffer, " \t\n\r");
                while (token != NULL && nova_aposta.qtd_n < MAX_NUMS) {
                    int valido = 1;
                    int len = strlen(token);
                    
                    // Validação de entrada: Verifica se o token contém apenas dígitos
                    for (int i = 0; i < len; i++) {
                        if (!isdigit(token[i])) {
                            valido = 0;
                            break;
                        }
                    }

                    if (valido && len > 0) {
                        nova_aposta.num[nova_aposta.qtd_n] = atoi(token);
                        nova_aposta.qtd_n++;
                    } else {
                        printf("[Aviso] Entrada invalida ignorada: '%s'\n", token);
                    }

                    token = strtok(NULL, " \t\n\r");
                }

                if (nova_aposta.qtd_n > 0) {
                    lista_apostas[total_apostas] = nova_aposta;
                    total_apostas++;
                    printf("[Aposta] Aposta %d registrada com %d numero(s).\n", 
                            total_apostas, nova_aposta.qtd_n);
                } else {
                    printf("[Aviso] Nenhuma aposta valida encontrada nesta mensagem.\n");
                }
            } else {
                printf("[Aposta] Limite maximo de apostas atingido.\n");
            }
        }

        // Libera o Mutex após a manipulação segura dos dados
        pthread_mutex_unlock(&m_loteria);
    }
    return NULL;
}

void* temporizador_sorteio(void* arg) {
    char msg_sorteio[2048]; // Buffer grande para caber todo o "boletim"
    char temp[100];         // Buffer temporário para formatar números pequenos

    // Inicializa a semente aleatória usando a hora atual do sistema
    srand(time(NULL));

    while(1) {
        // Fragmentação do sleep para verificar desconexão do cliente de 1 em 1 segundo
        int sair_da_thread = 0;
        for(int s = 0; s < 60; s++) {
            pthread_mutex_lock(&m_loteria);
            if (cliente_conectado == 0) {
                sair_da_thread = 1;
            }
            pthread_mutex_unlock(&m_loteria);
            
            if (sair_da_thread) break;
            sleep(1);
        }
        
        // Se a flag marcou para sair durante o tempo de espera
        if (sair_da_thread) {
            break;
        }

        int sorteados[100]; // Vetor para guardar os números desta rodada
        int min = config_i;
        int max = config_f;
        int qtd = config_qtd;

        // Trava de segurança: se a quantidade de números exigida for maior
        // que o intervalo disponível, ajustamos para evitar um loop infinito
        if (qtd > (max - min + 1)) {
            qtd = max - min + 1;
        }

        for (int i = 0; i < qtd; i++) {
            int numero;
            int repetido;
            do {
                repetido = 0;
                // Fórmula para gerar número num intervalo fechado: [min, max]
                numero = min + rand() % (max - min + 1);
                
                // Verifica se o número já saiu nos sorteios anteriores deste ciclo
                for (int j = 0; j < i; j++) {
                    if (sorteados[j] == numero) {
                        repetido = 1;
                        break; // Para de procurar, já achou repetição
                    }
                }
            } while (repetido); // Se for repetido, gera outro número
            
            sorteados[i] = numero;
        }

        pthread_mutex_lock(&m_loteria);
        
        // Verifica conexão mais uma vez antes de processar/enviar
        if (cliente_conectado == 0) {
            pthread_mutex_unlock(&m_loteria);
            break;
        }

        // Limpa a string principal antes de começar a montá-la
        memset(msg_sorteio, 0, sizeof(msg_sorteio));
        strcat(msg_sorteio, "\n=== RESULTADO DA LOTERIA ===\nNumeros Sorteados: ");

        // Anexa os números sorteados ao texto
        for (int i = 0; i < qtd; i++) {
            sprintf(temp, "%d ", sorteados[i]);
            strcat(msg_sorteio, temp);
        }
        strcat(msg_sorteio, "\n\nSua Apuracao:\n");

        if (total_apostas == 0) {
            strcat(msg_sorteio, "-> Voce nao fez nenhuma aposta nesta rodada.\n");
        } else {
            // Percorre todas as apostas salvas
            for (int i = 0; i < total_apostas; i++) {
                int acertos = 0;
                sprintf(temp, "Aposta %d [ ", i + 1);
                strcat(msg_sorteio, temp);

                // Percorre os números de uma aposta específica
                for (int j = 0; j < lista_apostas[i].qtd_n; j++) {
                    int num_apostado = lista_apostas[i].num[j];
                    sprintf(temp, "%d ", num_apostado);
                    strcat(msg_sorteio, temp);

                    // Checa se esse número apostado está entre os sorteados
                    for (int k = 0; k < qtd; k++) {
                        if (num_apostado == sorteados[k]) {
                            acertos++;
                            break;
                        }
                    }
                }
                // Adiciona o resultado da aposta ao texto
                sprintf(temp, "] -> Voce acertou %d numero(s)!\n", acertos);
                strcat(msg_sorteio, temp);
            }
        }
        strcat(msg_sorteio, "============================\n");

        total_apostas = 0; 

        pthread_mutex_unlock(&m_loteria);

        send(clientSocket, msg_sorteio, strlen(msg_sorteio), 0);
    }
    
    printf("[Sistema] Thread de sorteio finalizada.\n");
    return NULL;
}

int main(int argc, char const* argv[])
{
    // Registra o tratador de sinal para garantir fechamento de sockets
    signal(SIGINT, handle_sigint);

    // Cria socket do servidor semelhante ao que foi feito no cliente
    servSockD = socket(AF_INET, SOCK_STREAM, 0);

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
    clientSocket = accept(servSockD, NULL, NULL);
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
    pthread_create(&thread_recebimento, NULL, receber_dados_cliente, NULL);
    
    // Criando a Thread 2 (temporizador de sorteio)
    pthread_create(&thread_sorteio, NULL, temporizador_sorteio, NULL);

    // O main aguarda pelo termino das threads
    pthread_join(thread_recebimento, NULL);
    pthread_join(thread_sorteio, NULL);

    // Encerramento Gracioso atingido (Fim do fluxo)
    printf("Sistema encerrado com sucesso.\n");
    close(clientSocket);
    close(servSockD);
    return 0;
}