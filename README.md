# Grupo05-Loteria

Projeto de Client/Server da disciplina de Redes de Computadores.

## Sobre o Projeto

Este projeto é uma aplicação Client/Server TCP multithreaded bidirecional e assíncrona, desenvolvida em linguagem C utilizando as APIs POSIX (Sockets e Pthreads). O objetivo principal é simular um sistema de loteria onde o servidor gerencia as rodadas de sorteio, valida e contabiliza as apostas enviadas pelo cliente. A comunicação ocorre em tempo real, sem que um lado bloqueie a execução do outro.

## Arquitetura

O sistema emprega uma arquitetura paralela fortemente baseada em threads para evitar bloqueios de I/O (Input/Output):

- **Cliente (2 Threads):**
  - **Thread de Envio (Leitura de Teclado):** Dedicada exclusivamente a capturar os dados do usuário (via `stdin`) e despachá-los pela rede.
  - **Thread de Recebimento:** Fica bloqueada na rede aguardando as respostas e os boletins de sorteio vindos do servidor para exibi-los na tela.
  
- **Servidor (2 Threads):**
  - **Thread de Recepção:** Lê o socket do cliente de forma contínua, extrai as configurações ou apostas recebidas e gerencia a estrutura de dados central utilizando mecanismos de sincronização (`Mutex`) para evitar concorrência.
  - **Thread Temporizadora (Sorteio):** Atua como o motor lógico da loteria. A cada 60 segundos, ela acorda, sorteia os números da rodada atual, apura os acertos do cliente, envia o boletim de resultado pela rede e zera as apostas.

- **Encerramento Gracioso (Graceful Shutdown):** O projeto implementa tratamento de sinais (`SIGINT`) e detecção de queda de conexão (`recv <= 0`). Ambos os lados garantem o fechamento dos *File Descriptors* dos sockets abertos e a sinalização adequada entre as threads para o encerramento completo e seguro da função `main`.

## Como Compilar

O projeto deve ser compilado em um ambiente UNIX/Linux (ou WSL) utilizando o compilador GCC. 

> **Atenção:** É obrigatório incluir a flag `-pthread` para realizar a linkagem correta da biblioteca POSIX Threads.

Abra o terminal na pasta raiz do projeto e execute os seguintes comandos:

```bash
# Compilando o Servidor
gcc Server.c -o Server -pthread

# Compilando o Cliente
gcc Client.c -o Client -pthread
```

## Como Executar

A ordem de inicialização é estrita: o Servidor deve ser instanciado antes para começar a escutar (Listen) na porta designada.

**1. Inicie o Servidor:**
Abra um terminal e rode o executável compilado:
```bash
./Server
```
*O servidor ficará aguardando a conexão do cliente.*

**2. Inicie o Cliente:**
Abra um segundo terminal (na mesma máquina) e rode o cliente:
```bash
./Client
```

## Como Usar (Comandos)

Uma vez conectado, o cliente pode interagir com a loteria. O sistema aceita dois tipos de entrada: **Comandos de Configuração** e **Apostas**.

### Comandos de Configuração
Você pode alterar as regras do sorteio enviando comandos iniciados por dois pontos (`:`).
- `:inicio <NUMERO>` - Define o número mínimo do intervalo de sorteio (Padrão: 0).
- `:fim <NUMERO>` - Define o número máximo do intervalo de sorteio (Padrão: 100).
- `:qtd <NUMERO>` - Define a quantidade de números que serão sorteados na rodada (Padrão: 5).

*Exemplo: Digite `:qtd 10` e pressione Enter.*

### Apostas
Qualquer outra entrada que contenha números separados por espaço será interpretada como uma aposta. 
A entrada passará por uma rigorosa validação: caracteres não-numéricos ou símbolos serão ignorados e descartados para evitar corrupção da aposta.

*Exemplo de Aposta Válida:*
```text
10 25 33 42 99
```

A cada 60 segundos, o Servidor encerrará o período de apostas, publicará o boletim com o resultado da loteria informando quantos números você acertou, e uma nova rodada será iniciada automaticamente!
