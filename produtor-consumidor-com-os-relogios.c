/*
 * Compilação: mpicc -o produtor-consumidor-com-os-relogios produtor-consumidor-com-os-relogios.c  -lpthread -lrt
 * Execução:   mpiexec -n 3 ./produtor-consumidor-com-os-relogios
 * ## Integrantes
 * Jorge Vinícius Lourenço Santos
 * Matheus de Farias Santos
 * Adailton Moura da Silva
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h>

#define TAMANHO_FILA 10

typedef struct RelogioProcesso
{
    int tempo[3];
} RelogioProcesso;

typedef struct Mensagem
{
    RelogioProcesso relogio;
    int destino;
    int origem;
} Mensagem;

RelogioProcesso relogioGlobal = {{0, 0, 0}};

int tamanhoFilaEntrada = 0;
pthread_cond_t condFilaCheiaEntrada;
pthread_cond_t condFilaVaziaEntrada;
pthread_mutex_t mutexFilaEntrada;
RelogioProcesso filaEntrada[TAMANHO_FILA];

int tamanhoFilaSaida = 0;
pthread_cond_t condFilaCheiaSaida;
pthread_cond_t condFilaVaziaSaida;
pthread_mutex_t mutexFilaSaida;
Mensagem filaSaida[TAMANHO_FILA];

void imprimirRelogio(RelogioProcesso *relogio, int processo)
{
    printf("Processo: %d, Relogio: (%d, %d, %d)\n", processo, relogio->tempo[0], relogio->tempo[1], relogio->tempo[2]);
}

void eventoInterno(int pid, RelogioProcesso *relogio)
{
    relogio->tempo[pid]++;
}

void enviarMensagem(int remetente, int destinatario)
{
    pthread_mutex_lock(&mutexFilaSaida);
    relogioGlobal.tempo[remetente]++;
    imprimirRelogio(&relogioGlobal, remetente);

    while (tamanhoFilaSaida == TAMANHO_FILA)
    {
        pthread_cond_wait(&condFilaCheiaSaida, &mutexFilaSaida);
    }

    Mensagem *msg = (Mensagem *)malloc(sizeof(Mensagem));
    if (msg == NULL)
    {
        perror("Erro ao alocar mem�ria para mensagem");
        exit(EXIT_FAILURE);
    }

    msg->relogio = relogioGlobal;
    msg->origem = remetente;
    msg->destino = destinatario;

    filaSaida[tamanhoFilaSaida] = *msg;
    tamanhoFilaSaida++;

    pthread_mutex_unlock(&mutexFilaSaida);
    pthread_cond_signal(&condFilaVaziaSaida);
}

void enviarMensagemSaida()
{
    pthread_mutex_lock(&mutexFilaSaida);

    while (tamanhoFilaSaida == 0)
    {
        pthread_cond_wait(&condFilaVaziaSaida, &mutexFilaSaida);
    }

    Mensagem msg = filaSaida[0];
    for (int i = 0; i < tamanhoFilaSaida - 1; i++)
    {
        filaSaida[i] = filaSaida[i + 1];
    }
    tamanhoFilaSaida--;

    int *valoresRelogio;
    valoresRelogio = calloc(3, sizeof(int));
    if (valoresRelogio == NULL)
    {
        perror("Erro ao alocar mem�ria para valores do rel�gio");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 3; i++)
    {
        valoresRelogio[i] = msg.relogio.tempo[i];
    }

    MPI_Send(valoresRelogio, 3, MPI_INT, msg.destino, msg.origem, MPI_COMM_WORLD);

    free(valoresRelogio);

    pthread_mutex_unlock(&mutexFilaSaida);
    pthread_cond_signal(&condFilaCheiaSaida);
}

void receberMensagemEntrada()
{
    int *valoresRelogio;
    valoresRelogio = calloc(3, sizeof(int));
    if (valoresRelogio == NULL)
    {
        perror("Erro ao alocar mem�ria para valores do rel�gio");
        exit(EXIT_FAILURE);
    }

    RelogioProcesso *relogio = (RelogioProcesso *)malloc(sizeof(RelogioProcesso));
    if (relogio == NULL)
    {
        perror("Erro ao alocar mem�ria para rel�gio");
        exit(EXIT_FAILURE);
    }

    MPI_Recv(valoresRelogio, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < 3; i++)
    {
        relogio->tempo[i] = valoresRelogio[i];
    }
    free(valoresRelogio);

    pthread_mutex_lock(&mutexFilaEntrada);

    while (tamanhoFilaEntrada == TAMANHO_FILA)
    {
        pthread_cond_wait(&condFilaCheiaEntrada, &mutexFilaEntrada);
    }

    filaEntrada[tamanhoFilaEntrada] = *relogio;
    tamanhoFilaEntrada++;

    pthread_mutex_unlock(&mutexFilaEntrada);
    pthread_cond_signal(&condFilaVaziaEntrada);
}

void receberMensagem(int processo)
{
    pthread_mutex_lock(&mutexFilaEntrada);
    relogioGlobal.tempo[processo]++;

    while (tamanhoFilaEntrada == 0)
    {
        pthread_cond_wait(&condFilaVaziaEntrada, &mutexFilaEntrada);
    }

    RelogioProcesso relogio = filaEntrada[0];
    for (int i = 0; i < tamanhoFilaEntrada - 1; i++)
    {
        filaEntrada[i] = filaEntrada[i + 1];
    }
    tamanhoFilaEntrada--;

    for (int i = 0; i < 3; i++)
    {
        if (relogio.tempo[i] > relogioGlobal.tempo[i])
        {
            relogioGlobal.tempo[i] = relogio.tempo[i];
        }
    }

    imprimirRelogio(&relogioGlobal, processo);

    pthread_mutex_unlock(&mutexFilaEntrada);
    pthread_cond_signal(&condFilaCheiaEntrada);
}

void *threadRelogioProcesso(void *arg)
{
    long idProcesso = (long)arg;
    if (idProcesso == 0)
    {
        eventoInterno(0, &relogioGlobal);
        imprimirRelogio(&relogioGlobal, 0);

        enviarMensagem(0, 1);

        receberMensagem(0);

        enviarMensagem(0, 2);

        receberMensagem(0);

        enviarMensagem(0, 1);

        eventoInterno(0, &relogioGlobal);
        imprimirRelogio(&relogioGlobal, 0);
    }

    if (idProcesso == 1)
    {
        enviarMensagem(1, 0);

        receberMensagem(1);

        receberMensagem(1);
    }

    if (idProcesso == 2)
    {
        eventoInterno(2, &relogioGlobal);
        imprimirRelogio(&relogioGlobal, 2);

        enviarMensagem(2, 0);

        receberMensagem(2);
    }
    return NULL;
}

void *threadEnvioMensagem(void *arg)
{
    long idProcesso = (long)arg;
    while (1)
    {
        enviarMensagemSaida();
    }
    return NULL;
}

void *threadRecebimentoMensagem(void *arg)
{
    long idProcesso = (long)arg;
    while (1)
    {
        receberMensagemEntrada();
    }
    return NULL;
}

void processo(long idProcesso)
{
    pthread_t tEnvioMensagem;
    pthread_t tRecebimentoMensagem;
    pthread_t tRelogioProcesso;

    pthread_cond_init(&condFilaCheiaEntrada, NULL);
    pthread_cond_init(&condFilaVaziaEntrada, NULL);
    pthread_cond_init(&condFilaCheiaSaida, NULL);
    pthread_cond_init(&condFilaVaziaSaida, NULL);
    pthread_mutex_init(&mutexFilaEntrada, NULL);
    pthread_mutex_init(&mutexFilaSaida, NULL);

    if (pthread_create(&tRelogioProcesso, NULL, &threadRelogioProcesso, (void *)idProcesso) != 0)
    {
        fprintf(stderr, "Erro ao criar a thread do processo %ld\n", idProcesso);
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&tRecebimentoMensagem, NULL, &threadRecebimentoMensagem, (void *)idProcesso) != 0)
    {
        fprintf(stderr, "Erro ao criar a thread do processo %ld\n", idProcesso);
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&tEnvioMensagem, NULL, &threadEnvioMensagem, (void *)idProcesso) != 0)
    {
        fprintf(stderr, "Erro ao criar a thread do processo %ld\n", idProcesso);
        exit(EXIT_FAILURE);
    }

    if (pthread_join(tRelogioProcesso, NULL) != 0)
    {
        fprintf(stderr, "Erro ao aguardar a thread do processo %ld\n", idProcesso);
        exit(EXIT_FAILURE);
    }
    if (pthread_join(tRecebimentoMensagem, NULL) != 0)
    {
        fprintf(stderr, "Erro ao aguardar a thread do processo %ld\n", idProcesso);
        exit(EXIT_FAILURE);
    }
    if (pthread_join(tEnvioMensagem, NULL) != 0)
    {
        fprintf(stderr, "Erro ao aguardar a thread do processo %ld\n", idProcesso);
        exit(EXIT_FAILURE);
    }

    pthread_cond_destroy(&condFilaCheiaEntrada);
    pthread_cond_destroy(&condFilaVaziaEntrada);
    pthread_cond_destroy(&condFilaCheiaSaida);
    pthread_cond_destroy(&condFilaVaziaSaida);
    pthread_mutex_destroy(&mutexFilaEntrada);
    pthread_mutex_destroy(&mutexFilaSaida);
}

int main(void)
{
    int meu_rank;

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &meu_rank);

    if (meu_rank == 0)
    {
        processo(0);
    }
    else if (meu_rank == 1)
    {
        processo(1);
    }
    else if (meu_rank == 2)
    {
        processo(2);
    }

    /* Finaliza MPI */
    MPI_Finalize();

    return 0;
}
