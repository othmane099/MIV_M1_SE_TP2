#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>

#define QUEUE_SIZE 5

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
};

void sem_init(int semid, int semaphore_number, int initial_value)
{
    union semun argument;
    argument.val = initial_value;
    /*
    * SETVAL (8): Cette action est l'initialisation de la valeur du semaphore.
    * La valeur semval du semaphore de numero semnum est mise a arg.val .
    */
    if (semctl(semid, semaphore_number, SETVAL, argument) == -1)
    {
        perror("semctl\n");
        error(1);
    }
}

void sem_action(int semid, int sem_number, int sem_operation, struct sembuf *ptr)
{
    ptr->sem_num = sem_number;   /* numero de semaphore */
    ptr->sem_op = sem_operation; /* operation de (decrementation/incrementation) */
    ptr->sem_flg = SEM_UNDO;     /* pour defaire les operations */
    if (semop(semid, ptr, 1) == -1)
    {
        perror("semop()");
        exit(1);
    }
}

struct Tempon
{
    pid_t buffer[QUEUE_SIZE];
    bool isAnyInQueue;
    unsigned beginIndex;
    unsigned endIndex;
};

struct my_msgbuf
{
    long mtype;
    char mtext[200];
};

int shmid;
int semid;
int msqid;
struct Tempon *tampon;
struct sembuf sem = {0, 0};
struct my_msgbuf buf;

void prodCons();
void cons();
void prod();

enum
{
    FULL,
    EMPT
};

int main(int argc, char const *argv[])
{
    int pid;
    int i = 0;

    key_t key = ftok(argv[0], 1);

    /*
     * Allouer de l’espace pour le tampon
     */

    if ((shmid = shmget(key, sizeof(struct Tempon), S_IRUSR | S_IWUSR | IPC_CREAT)) == -1)
    {
        perror("Echec de shmget\n");
        error(1);
    }

    /*
     * Creation de 2 semaphores
     */

    if ((semid = semget(key, 2, S_IRUSR | S_IWUSR | IPC_CREAT)) == -1)
    {
        perror("creation des semaphores non effectuee\n");
        error(1);
    }

    /*
     * Creation de la file de messages
     */

    if ((msqid = msgget(key, 0644 | IPC_CREAT)) == -1)
    {
        perror("creation de la file impossible\n");
        exit(1);
    }
    buf.mtype = 1;

    /*
     * Attachement du processus a la zone de memoire
     */
    if ((tampon = shmat(shmid, NULL, 0)) == -1)
    {
        perror("attachement impossible\n");
        error(1);
    }

    tampon->beginIndex = 0;
    tampon->endIndex = 0;
    tampon->isAnyInQueue = false;

    sem_init(semid, FULL, 0);
    sem_init(semid, EMPT, QUEUE_SIZE);

    /*
	 * Call Producteur
	 */

    if (fork() == 0)
        prod();

    /*
	 * producteurConsomateur
	 */

    if (fork() == 0)
        prodCons();

    /*
	 * Consomateur
	 */

    if (fork() == 0)
        cons();

    /*
	 * Affichages des etats du tampon
	 */

    int elem_counter;
    while (1)
    {
        elem_counter = 0;

        /* If tampon not empty */
        if (tampon->isAnyInQueue)
        {
            i = tampon->beginIndex;
            printf("\nQueue content:\n");
            if (tampon->beginIndex == tampon->endIndex)
                printf("(Queue full)\n");
            do
            {
                elem_counter++;
                printf("%d\t", tampon->buffer[i]);
                i = (i + 1) % QUEUE_SIZE;
            } while (i != tampon->endIndex);
            printf("\t__%d__ elements in the tampon\n", elem_counter);
        }
        else
        {
            printf("\nEmpty buffer...\n");
        }
        sleep(1);
    }
    return 0;
}

void prod()
{
    char str[200];
    int cpt = 1;
    while (cpt <= 20)
    {
        sprintf(str, "%d", cpt);
        strncpy(buf.mtext, str, 2);
        int len = strlen(buf.mtext);
        /*
         * envoi d'un message
         */
        if (msgsnd(msqid, &buf, len + 1, 0) == -1) { /* +1 for '\0' */
            perror("envoi impossible\n");
            exit(1);
        }
        printf("\nProd ==> %d\n", cpt);
        cpt++;
        sleep(1);
    }
    printf("Prod Termine\n");
    exit(0);
}
void prodCons()
{
    int cpt;
    /*
     * attachement du processus a la zone de memoire
	 */
    if ((tampon = shmat(shmid, NULL, 0)) == -1)
    {
        perror("attachement impossible\n");
        error(1);
    }

    do
    {
        sem_action(semid, EMPT, -1, &sem);

        /*
         * reception d'un message
         */
        if (msgrcv(msqid, &buf, sizeof buf.mtext, 0, 0) == -1)
        {
            perror("reception impossible\n");
            exit(1);
        }

        cpt = atoi(buf.mtext);

        tampon->buffer[tampon->endIndex] = atoi(buf.mtext);
        tampon->endIndex = (tampon->endIndex + 1) % (QUEUE_SIZE);

        /* There is at least one element in the tampon */
        tampon->isAnyInQueue = true;
        sem_action(semid, FULL, 1, &sem);
        printf("\nProdCons ==> %d\n", cpt);
        sleep(1);
    } while (cpt < 20);

    /* 
     * Detruire la file de messages
     */

    msgctl(msqid, IPC_RMID, NULL);
    exit(0);
}

void cons()
{
    int value;
    /*
     * attachement du processus a la zone de memoire
	 */
    if ((tampon = shmat(shmid, NULL, 0)) == -1)
    {
        perror("attachement impossible\n");
        error(1);
    }

    do
    {
        sem_action(semid, FULL, -1, &sem);

        value = tampon->buffer[tampon->beginIndex];
        tampon->beginIndex = (tampon->beginIndex + 1) % (QUEUE_SIZE);

        /* If there was the last element, clear the flag */
        if (tampon->beginIndex == tampon->endIndex)
            tampon->isAnyInQueue = false;
        sem_action(semid, EMPT, 1, &sem);
        printf("\nCons ==> %d\n", value);

        sleep(1);
    } while (value < 20);

    /*
     * detachement du segment
     */
    if (shmdt(tampon) == -1)
    {
        perror("detachement impossible");
        exit(1);
    }

    /*
     * destruction l'ensemble de semaphores
     */
    semctl(semid, 0, IPC_RMID);

    /*
     * destruction du segment
     */
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("destruction impossible");
        exit(1);
    }
    exit(0);
}
