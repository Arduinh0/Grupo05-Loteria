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

// FASE 2: Estrutura de contexto para garantir o Isolamento de Estado de cada cliente
typedef struct {
    int clientSocket;
    int config_i;
    int config_f;
    int config_qtd;
    Aposta lista_apostas[MAX_APOSTAS];
    int total_apostas;
    int cliente_conectado;
    pthread_mutex_t m_cliente; // Mutex exclusivo para este cliente
} ClienteContext;

// FASE 2: Variáveis globais de controle do servidor multi-client
int clientes_conectados = 0;
int limite_clientes = 0;
pthread_mutex_t m_geral = PTHREAD_MUTEX_INITIALIZER; // Mutex global apenas para controle de conexões
int servSockD = -1;

// Tratador de sinal para grateful ending via Ctrl+C (Atualizado para fechar apenas servSockD)
void handle_sigint(int sig) {
    printf("\n[Sinal] Capturado SIGINT (Ctrl+C). Fechando socket do servidor e encerrando...\n");
    if (servSockD != -1) close(servSockD);
    exit(0);
}

// Thread do Temporizador: Fica responsável apenas pelo envio e cálculo periódico
void* temporizador_sorteio(void* arg) {
    ClienteContext* ctx = (ClienteContext*)arg;
    char msg_sorteio[2048]; // Buffer grande para caber todo o "boletim"
    char temp[100];         // Buffer temporário para formatar números pequenos

    // Inicializa a semente aleatória usando a hora atual do sistema
    srand(time(NULL));

    while(1) {
        // Fragmentação do sleep para verificar desconexão do cliente de 1 em 1 segundo
        int sair_da_thread = 0;
        for(int s = 0; s < 60; s++) {
            pthread_mutex_lock(&ctx->m_cliente);
            if (ctx->cliente_conectado == 0) {
                sair_da_thread = 1;
            }
            pthread_mutex_unlock(&ctx->m_cliente);
            
            if (sair_da_thread) break;
            sleep(1);
        }
        
        // Se a flag marcou para sair durante o tempo de espera
        if (sair_da_thread) {
            break;
        }

        int sorteados[100]; // Vetor para guardar os números desta rodada
        
        pthread_mutex_lock(&ctx->m_cliente);
        
        // Verifica conexão mais uma vez antes de processar/enviar
        if (ctx->cliente_conectado == 0) {
            pthread_mutex_unlock(&ctx->m_cliente);
            break;
        }

        // Leitura segura das configurações do cliente
        int min = ctx->config_i;
        int max = ctx->config_f;
        int qtd = ctx->config_qtd;

        // Trava de segurança: se a quantidade de números exigida for maior
        // que o intervalo disponível, ajustamos para evitar um loop infinito
        if (qtd > (max - min + 1)) {
            qtd = max - min + 1;
        }
        
        pthread_mutex_unlock(&ctx->m_cliente);

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

        pthread_mutex_lock(&ctx->m_cliente);
        
        if (ctx->cliente_conectado == 0) {
            pthread_mutex_unlock(&ctx->m_cliente);
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

        if (ctx->total_apostas == 0) {
            strcat(msg_sorteio, "-> Voce nao fez nenhuma aposta nesta rodada.\n");
        } else {
            // Percorre todas as apostas salvas
            for (int i = 0; i < ctx->total_apostas; i++) {
                int acertos = 0;
                sprintf(temp, "Aposta %d [ ", i + 1);
                strcat(msg_sorteio, temp);

                // Percorre os números de uma aposta específica
                for (int j = 0; j < ctx->lista_apostas[i].qtd_n; j++) {
                    int num_apostado = ctx->lista_apostas[i].num[j];
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

        ctx->total_apostas = 0; 

        pthread_mutex_unlock(&ctx->m_cliente);

        send(ctx->clientSocket, msg_sorteio, strlen(msg_sorteio), 0);
    }
    
    printf("[Sistema] Thread de sorteio finalizada para o socket %d.\n", ctx->clientSocket);
    return NULL;
}

// FASE 2: Working Thread principal para lidar com o ciclo de vida deste cliente
void* gerenciar_cliente(void* arg) {
    ClienteContext* ctx = (ClienteContext*)arg;
    char buffer[255];
    int bytes_recebidos;
    pthread_t thread_sorteio;

    // FASE 2: Inicia o timer da loteria repassando o próprio contexto (trabalho independente)
    pthread_create(&thread_sorteio, NULL, temporizador_sorteio, ctx);

    while (1) {
        bytes_recebidos = recv(ctx->clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recebidos <= 0) {
            printf("\n[Sistema] Cliente do socket %d desconectado.\n", ctx->clientSocket);
            pthread_mutex_lock(&ctx->m_cliente);
            ctx->cliente_conectado = 0;
            pthread_mutex_unlock(&ctx->m_cliente);
            break; 
        }

        buffer[bytes_recebidos] = '\0';

        // Checa encerramento voluntário
        if (strncmp(buffer, ":quit", 5) == 0) {
            printf("\n[Sistema] Socket %d enviou :quit. Desconectando...\n", ctx->clientSocket);
            pthread_mutex_lock(&ctx->m_cliente);
            ctx->cliente_conectado = 0;
            pthread_mutex_unlock(&ctx->m_cliente);
            break; 
        }
        
        pthread_mutex_lock(&ctx->m_cliente);

        if (buffer[0] == ':') {
            int valor;
            if (sscanf(buffer, ":inicio %d", &valor) == 1) {
                ctx->config_i = valor;
                printf("[Config Cliente %d] Inicio alterado para: %d\n", ctx->clientSocket, ctx->config_i);
            } 
            else if (sscanf(buffer, ":fim %d", &valor) == 1) {
                ctx->config_f = valor;
                printf("[Config Cliente %d] Fim alterado para: %d\n", ctx->clientSocket, ctx->config_f);
            } 
            else if (sscanf(buffer, ":qtd %d", &valor) == 1) {
                ctx->config_qtd = valor;
                printf("[Config Cliente %d] Qtd de numeros sorteados alterada para: %d\n", ctx->clientSocket, ctx->config_qtd);
            }
        } 
        else {
            if (ctx->total_apostas < MAX_APOSTAS) {
                Aposta nova_aposta;
                nova_aposta.qtd_n = 0;

                char* token = strtok(buffer, " \t\n\r");
                while (token != NULL && nova_aposta.qtd_n < MAX_NUMS) {
                    int valido = 1;
                    int len = strlen(token);
                    
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
                        printf("[Aviso Cliente %d] Entrada invalida ignorada: '%s'\n", ctx->clientSocket, token);
                    }

                    token = strtok(NULL, " \t\n\r");
                }

                if (nova_aposta.qtd_n > 0) {
                    ctx->lista_apostas[ctx->total_apostas] = nova_aposta;
                    ctx->total_apostas++;
                    printf("[Aposta Cliente %d] Aposta %d registrada com %d numero(s).\n", 
                            ctx->clientSocket, ctx->total_apostas, nova_aposta.qtd_n);
                } else {
                    printf("[Aviso Cliente %d] Nenhuma aposta valida encontrada nesta mensagem.\n", ctx->clientSocket);
                }
            } else {
                printf("[Aposta Cliente %d] Limite maximo de apostas atingido.\n", ctx->clientSocket);
            }
        }
        pthread_mutex_unlock(&ctx->m_cliente);
    }
    
    // FASE 2: Prevenção de Segfault. Aguardamos a finalização do timer antes de dar o free.
    pthread_join(thread_sorteio, NULL);

    // Fechamento da conexão e desalocação do contexto
    close(ctx->clientSocket);
    pthread_mutex_destroy(&ctx->m_cliente);
    free(ctx);

    // Redução da contagem de clientes com segurança global
    pthread_mutex_lock(&m_geral);
    clientes_conectados--;
    printf("[Sistema] Conexão encerrada. Clientes ativos: %d/%d\n", clientes_conectados, limite_clientes);
    pthread_mutex_unlock(&m_geral);

    return NULL;
}

int main(int argc, char const* argv[])
{
    // FASE 2: Validação e Limite de Clientes
    if (argc < 2) {
        printf("Uso: %s <limite_de_clientes_simultaneos>\n", argv[0]);
        exit(1);
    }
    
    limite_clientes = atoi(argv[1]);
    if (limite_clientes <= 0) {
        printf("Erro: O limite de clientes deve ser um numero maior que zero.\n");
        exit(1);
    }

    // Registra o tratador de sinal para garantir fechamento de sockets
    signal(SIGINT, handle_sigint);

    // Cria socket do servidor semelhante ao que foi feito no cliente
    servSockD = socket(AF_INET, SOCK_STREAM, 0);

    // Configuração do socket para reaproveitamento do porto, útil para testes
    int opt = 1;
    setsockopt(servSockD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Define o endereco do servidor
    struct sockaddr_in servAddr;

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(9001);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    // Faz o bind do socket com o IP e porta especificados
    bind(servSockD, (struct sockaddr*)&servAddr, sizeof(servAddr));

    // Ouve por conexões com tamanho máximo da fila de accept no UNIX
    listen(servSockD, SOMAXCONN);

    printf("Servidor Multi-client da Loteria iniciado!\n");
    printf("Aguardando por conexoes (Limite de %d clientes simultaneos na porta 9001)...\n", limite_clientes);
    
    // FASE 2: O main agora roda um loop infinito para processamento contínuo do accept
    while (1) {
        int socket_novo_cliente = accept(servSockD, NULL, NULL);
        if (socket_novo_cliente < 0) {
            continue; // Se falhou no accept, ignora e tenta o próximo
        }

        // Bloqueio de vagas e verificação de limite
        pthread_mutex_lock(&m_geral);
        if (clientes_conectados >= limite_clientes) {
            pthread_mutex_unlock(&m_geral);
            printf("[Sistema] Nova tentativa de conexao rejeitada. Limite excedido.\n");
            
            char* msg_limite = "Limite de clientes excedido. Tente novamente mais tarde.\n";
            send(socket_novo_cliente, msg_limite, strlen(msg_limite), 0);
            close(socket_novo_cliente);
            continue;
        }

        // Há vaga disponível
        clientes_conectados++;
        printf("\n[Sistema] Um novo cliente foi conectado! Clientes ativos: %d/%d\n", clientes_conectados, limite_clientes);
        pthread_mutex_unlock(&m_geral);

        // FASE 2: Aloque o contexto e preencha as variáveis independentes para a thread
        ClienteContext* ctx = (ClienteContext*) malloc(sizeof(ClienteContext));
        ctx->clientSocket = socket_novo_cliente;
        ctx->config_i = 0;
        ctx->config_f = 100;
        ctx->config_qtd = 5;
        ctx->total_apostas = 0;
        ctx->cliente_conectado = 1;
        pthread_mutex_init(&ctx->m_cliente, NULL);

        // Envia mensagem inicial
        time_t t = time(NULL); 
        struct tm *tm_info = localtime(&t);
        char serMsg[255]; 
        sprintf(serMsg, "%02d:%02d:%02d: CONECTADO!!", 
                tm_info->tm_hour, 
                tm_info->tm_min, 
                tm_info->tm_sec);
        send(ctx->clientSocket, serMsg, strlen(serMsg), 0);

        // Dispara a Working Thread
        pthread_t thread_cliente;
        pthread_create(&thread_cliente, NULL, gerenciar_cliente, ctx);
        
        // Faz o detach para que a thread seja autodestrutiva, evitando vazamento de recursos no S.O.
        pthread_detach(thread_cliente);
    }

    close(servSockD);
    return 0;
}
