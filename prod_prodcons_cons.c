#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>

#define T 5
#define N 20

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
};

struct Tampon
{
    pid_t buffer[T];
    int existeElement;
    unsigned dbt;
    unsigned fin;
};

struct my_msgbuf
{
    long mtype;
    char mtext[200];
};

int shmid;
int semid;
int msqid;
struct Tampon *tampon;
struct sembuf sem = {0, 0, 0};
struct my_msgbuf buf;

void prodCons();
void cons();
void prod();
void sem_init(int semid, int num, int val);
void sem_action(int semid, int num, int sem_operation, struct sembuf *p);

enum
{
    FULL,
    EMPT,
    PRIV
};

int main(int argc, char const *argv[])
{

    key_t key = ftok(argv[0], 2);

    /*
     * Allouer de l’espace pour le tampon
     */

    if ((shmid = shmget(key, sizeof(struct Tampon), S_IRUSR | S_IWUSR | IPC_CREAT)) == -1)
    {
        perror("Echec de shmget\n");
        error(1);
    }

    /*
     * Creation de 3 semaphores
     */

    if ((semid = semget(key, 3, S_IRUSR | S_IWUSR | IPC_CREAT)) == -1)
    {
        perror("creation des semaphores non effectuee\n");
        error(1);
    }

    /*
     * Creation de la file de messages
     */

    if ((msqid = msgget(key, S_IRUSR | S_IWUSR | IPC_CREAT)) == -1)
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

    tampon->dbt = 0;
    tampon->fin = 0;
    tampon->existeElement = 0;

    sem_init(semid, FULL, 0);
    sem_init(semid, EMPT, T);
    sem_init(semid, PRIV, 0);

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

    int cpt;
    int i = 0;
    while (1)
    {
        cpt = 0;

        if (tampon->existeElement)
        {
            i = tampon->dbt;
            printf("\nTampon : ");
            do
            {
                cpt++;
                printf("%d\t", tampon->buffer[i]);
                i = (i + 1) % T;
            } while (i != tampon->fin);
            printf("\n");
        }
        else
            printf("\nTampon est vide !!!\n");

        sleep(1);
    }
    return 0;
}

void prod()
{
    char str[200];
    int cpt = 1;
    while (cpt <= N)
    {
        sprintf(str, "%d", cpt);
        strncpy(buf.mtext, str, 2);
        int len = strlen(buf.mtext);
        /*
         * envoi d'un message
         */
        if (msgsnd(msqid, &buf, len + 1, 0) == -1)
        { /* +1 for '\0' */
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

        tampon->buffer[tampon->fin] = atoi(buf.mtext);
        tampon->fin = (tampon->fin + 1) % (T);

        tampon->existeElement = 1;
        sem_action(semid, FULL, 1, &sem);
        printf("\nProdCons ==> %d\n", cpt);
        sleep(1);
    } while (cpt < N);

    /* 
     * Detruire la file de messages
     */
    sem_action(semid, PRIV, -1, &sem);
    msgctl(msqid, IPC_RMID, NULL);
    printf("\nprodCons est termine\n");
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

        value = tampon->buffer[tampon->dbt];
        tampon->dbt = (tampon->dbt + 1) % (T);

        if (tampon->dbt == tampon->fin)
            tampon->existeElement = 0;
        sem_action(semid, EMPT, 1, &sem);
        printf("\nCons ==> %d\n", value);

        sleep(2);
    } while (value < N);

    sem_action(semid, PRIV, 1, &sem);

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
    printf("\nCons est termine\n");
    exit(0);
}

void sem_init(int semid, int num, int val)
{
    union semun arg;
    arg.val = val;

    if (semctl(semid, num, SETVAL, arg) == -1)
    {
        perror("semctl\n");
        error(1);
    }
}

void sem_action(int semid, int num, int opr, struct sembuf *p)
{
    p->sem_num = num;      /* numero de semaphore */
    p->sem_op = opr;       /* operation de (decrementation/incrementation) */
    p->sem_flg = SEM_UNDO; /* pour defaire les operations */
    if (semop(semid, p, 1) == -1)
    {
        perror("semop()");
        exit(1);
    }
}
