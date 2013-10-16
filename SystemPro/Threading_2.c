#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

typedef struct pass{
    int person_cnt[3];
    int total;
    int exit;
    int entered;
    int sema_val;
}pass;

typedef struct human{
    int type;
    int exited;
}human;

// global declarations {{{
sem_t semaphore;
sem_t arrival[3];
pass *group;
human *person;
int thrd_cnt;
int totalPeople;
int tpeople;
int deadlock;
int type[] = {0,1,2};
const char *person_typ[3];
// }}}

void initalise_group(int x)
{
    if(x==0){
        group->person_cnt[0] = 0;
        group->person_cnt[1] = 0;
        group->person_cnt[2] = 0;
        group->total         = 0;
        group->exit          = 0;
        group->entered       = 0;
        return;
    }
}

int check_num(int type)
{
    // calulate the number of people left for a type
    int i, cnt=0;
    for (i=0; i<tpeople; i++)
    {
        if ( person[i].type == type && person[i].exited==0)
            cnt += 1;
    }
    return cnt;
}

int check_deadlock(int geeks, int non_geeks, int singers)
{
    if (geeks==3 && non_geeks==1 && singers==0)
        return 1;
    if (geeks==1 && non_geeks==3 && singers==0)
        return 1;
    if (geeks+non_geeks+singers < 4)
        return 1;

    return 0;
}

int check_starvation(int type)
{
    // check if the current state will cause startvation
    int geeks = check_num(0);
    int non_geeks = check_num(1);
    int singers = check_num(2);
    if ( geeks>3 && non_geeks == 1 && singers == 0 && type==1)
        return 1;
    else if (geeks==1 && non_geeks > 3 && singers == 0 &&  type==0)
        return 1;
    else if (geeks==3 && non_geeks==3 && singers==0)
    {
        if (group->person_cnt[0]==2 && group->person_cnt[1]<2 && type==0)
            return 1;
        else if (group->person_cnt[0]<2 && group->person_cnt[1]==2 && type==1)
            return 1;
    }
    return 0;
}

int prepare_group(int type)
{
    int flag = 0;

    if ( type == 0 ) {

        if (group->person_cnt[1] > 2)
            return -1;
        else if (group->person_cnt[type]==2 && group->person_cnt[1]==1)
            return -1;
        else if (group->person_cnt[1]<=2 && group->person_cnt[type]<2)
            flag = 1;
        else if (group->person_cnt[type] < 2)
            flag = 1;
        else if (group->person_cnt[type] >= 2)
            flag = 1;

        if (flag==1){
            group->person_cnt[type] += 1;
            group->total += 1;
            totalPeople -= 1;
            return 1;
        }
        else
            return -1;
    }
    else if ( type == 1 ) {
        if (group->person_cnt[0] > 2)
            return -1;
        else if (group->person_cnt[type]==2 && group->person_cnt[0]==1)
            return -1;
        else if (group->person_cnt[type]<=2 && group->person_cnt[0]<2)
            flag = 1;
        else if (group->person_cnt[type] < 2)
            flag = 1;
        else if (group->person_cnt[type] >= 2)
            flag = 1;

        if (flag==1){
            group->person_cnt[type] += 1;
            group->total += 1;
            totalPeople -= 1;
            return 1;
        }
        else
            return -1;
    }
    else if ( type == 2 ) {
        if (group->person_cnt[type]==0){
            group->person_cnt[type] += 1;
            group->total += 1;
            totalPeople -= 1;
            return 1;
        }
        else
            return -1;
    }
    return -1;
}

int tmp_thrd_cnt=0;
void * travel(void * arg)
{
    human *person = (human *) arg;
    int value, allow=-1;
    int tmp;
    pid_t thid = pthread_self();
    // check if the current thread can enter the group
    while (allow==-1 && deadlock==0){
        /*sem_getvalue( &arrival[0], &tmp);*/
        /*printf("[%d %d]", tmp, thid);*/
        sem_wait(&arrival[0]);
            int starve = check_starvation(person->type);
            if (group->total<4 && starve==0 && deadlock==0)
                allow = prepare_group(person->type);
            /*printf("[%d %d %d] ", person->type, allow, group->total);*/
        sem_post(&arrival[0]);
    }
    // wait till all the 4 people leave the group
    while(group->entered==4);

    sem_wait(&semaphore);
        // track the no of people that have entered the group
        sem_wait(&arrival[1]);
            group->entered += 1;
        sem_post(&arrival[1]);
        // wait till all 4 people have not arrived
        sem_getvalue( &semaphore, &value);
        while (group->total != 4 && deadlock==0 && value!=0)
            sem_getvalue(&semaphore, &value);

    sem_post(&semaphore);
    // track which thread has left the group
    sem_wait(&arrival[2]);
        group->exit += 1;
        person->exited = 1;
        if (allow!=-1)
            printf("%s arrived.\n", person_typ[person->type]);
        else
            printf("%s starved.\n", person_typ[person->type]);

        // reset the group when all 4 have left the group
        if(group->exit==4)
        {
            int geeks = check_num(0);
            int non_geeks = check_num(1);
            int singers = check_num(2);
            printf("\n***Batch exited  %d %d %d\n", geeks, non_geeks, singers);
            deadlock = check_deadlock(geeks, non_geeks, singers);
            initalise_group(0);
        }
        thrd_cnt -= 1;
    sem_post(&arrival[2]);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    // variable declarations {{{
    int err, i=1;
    int geeks     = atoi(argv[1]);
    int non_geeks = atoi(argv[2]);
    int singers   = atoi(argv[3]);
    int totalP    = geeks + non_geeks + singers;
    totalPeople   = totalP;
    tpeople       = totalP;
    thrd_cnt      = 0;
    deadlock      = 0;
    person_typ[0] = "Geek";
    person_typ[1] = "Non Geek";
    person_typ[2] = "Singer";

    // initialse group
    group  = (pass *)malloc(sizeof(pass));
    person = (human *)malloc(sizeof(human)*totalP);
    initalise_group(0);
    // randomly allocate 3 type of people
    for (i = 0; i < totalP; ++i)
    {
        int r = rand()%3;
        if (r==0 && geeks>0){
            person[i].type = 0;
            geeks -= 1;
        }
        else if (r==1 && non_geeks>0){
            person[i].type = 1;
            non_geeks -= 1;
        }
        else if (r==2 && singers>0){
            person[i].type = 2;
            singers -= 1;
        }
        else
            i-=1;
        person[i].exited = 0;
        /*printf("%d %d\n", i, person[i].type );*/
    }
    // create threads out of atoms
    pthread_t tid;
    // initalise semaphores
    sem_init(&semaphore, 0, 4 );
    sem_init(&arrival[0], 0, 1 );
    sem_init(&arrival[1], 0, 1 );
    sem_init(&arrival[2], 0, 1 );
     // }}} 
    // create threads for each person
    for (i=0; i<totalP; i++)
    {
        thrd_cnt += 1;
        tmp_thrd_cnt += 1;
        err = pthread_create(&tid, NULL, travel, &person[i] );
        if (err!=0)
        {
            i-= 1;
            thrd_cnt -= 1;
            printf("***Error creating thread\n");
        }
    }

    //wait for all thread to terminate
    while(thrd_cnt!=0);
    printf("Terminating...\n");
    return 0;
}
