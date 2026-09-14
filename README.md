# Grupo05-Loteria

Projeto de Client/Server da disciplina de Redes de Computadores.

## Sobre o Projeto

Este projeto é uma aplicação Client/Server TCP multithreaded bidirecional e assíncrona, desenvolvida em linguagem C utilizando as APIs POSIX (Sockets e Pthreads). O objetivo principal é simular um sistema de loteria onde o servidor gerencia as rodadas de sorteio, valida e contabiliza as apostas enviadas. A Fase 2 (atual) transforma o servidor em uma aplicação **Multi-client**, permitindo o atendimento de múltiplos clientes simultaneamente de forma totalmente independente.

## Arquitetura

O sistema emprega uma arquitetura paralela fortemente baseada em threads para evitar bloqueios de I/O (Input/Output) e garantir o isolamento de estado:

- **Cliente (2 Threads):**
  - **Thread de Envio (Leitura de Teclado):** Dedicada exclusivamente a capturar os dados do usuário (via `stdin`) e despachá-los pela rede. Intercepta comandos locais como o `:quit` para encerramento seguro.
  - **Thread de Recebimento:** Fica bloqueada na rede aguardando as respostas e os boletins de sorteio vindos do servidor para exibi-los na tela.
  
- **Servidor Multi-client (Fase 2):**
  - **Loop Principal:** O servidor escuta as requisições de conexão e gerencia um limite global de clientes. Cada novo cliente aprovado recebe um contexto isolado.
  - **Working Thread (Independente):** Cada cliente conectado ganha uma "Working Thread" própria com um contexto (struct de configurações e apostas) e um `Mutex` isolados. Essa thread recebe as mensagens daquele cliente específico.
  - **Thread Temporizadora (Sorteio):** Disparada e gerenciada pela Working Thread. Atua como o motor lógico da loteria independente para aquele cliente. A cada 60 segundos, ela sorteia os números, apura os acertos, envia o boletim de resultado pela rede e zera as apostas.

- **Encerramento Gracioso (Graceful Shutdown):** O projeto implementa tratamento de sinais (`SIGINT`), comandos (`:quit`) e detecção de queda de conexão. Ambos os lados garantem o fechamento dos sockets, desalocação de memória e o sincronismo com `pthread_join` para evitar vazamentos e falhas de segmentação.

## Como Compilar

O projeto deve ser compilado em um ambiente UNIX/Linux (ou WSL) utilizando o compilador GCC. 

> **Atenção:** É obrigatório incluir a flag `-pthread` (ou `-lpthread`) para realizar a linkagem correta da biblioteca POSIX Threads.

Abra o terminal na pasta raiz do projeto e execute os seguintes comandos:

```bash
# Compilando o Servidor (Fase 2)
gcc Server.c -o Server -pthread

# Compilando o Cliente
gcc Client.c -o Client -pthread
```

## Como Executar

A ordem de inicialização é estrita: o Servidor deve ser instanciado antes para começar a escutar as conexões.

**1. Inicie o Servidor:**
> **CRÍTICO:** O servidor não roda mais sozinho. Ele exige um argumento de linha de comando com o limite máximo de clientes simultâneos.

Abra um terminal e rode o executável compilado com o limite desejado (ex: limite de 3 clientes):
```bash
./Server 3
```
*O servidor ficará aguardando as conexões dos clientes.*

**2. Inicie o Cliente(s):**
Abra novos terminais e rode o cliente:
```bash
./Client
```

## Como Usar (Comandos)

Uma vez conectado, o cliente pode interagir com a loteria. O sistema aceita três tipos de entrada: **Comandos de Configuração**, **Apostas** e **Comando de Saída**.

### Comandos de Configuração
Você pode alterar as regras do sorteio enviando comandos iniciados por dois pontos (`:`).
- `:inicio <NUMERO>` - Define o número mínimo do intervalo de sorteio (Padrão: 0).
- `:fim <NUMERO>` - Define o número máximo do intervalo de sorteio (Padrão: 100).
- `:qtd <NUMERO>` - Define a quantidade de números que serão sorteados na rodada (Padrão: 5).

### Comando de Saída
- `:quit` - Encerra localmente o cliente e avisa o servidor. O servidor irá liberar a vaga global para um novo cliente, encerrar o ciclo do temporizador e desalocar a memória de forma segura.

### Apostas
Qualquer outra entrada que contenha números separados por espaço será interpretada como uma aposta. 
A entrada passará por uma rigorosa validação: caracteres não-numéricos ou símbolos serão ignorados e descartados para evitar corrupção da aposta.

*Exemplo de Aposta Válida:*
```text
10 25 33 42 99
```

A cada 60 segundos, o Servidor encerrará o período de apostas daquele cliente, publicará o boletim com o resultado da loteria, e uma nova rodada independente será iniciada automaticamente!
