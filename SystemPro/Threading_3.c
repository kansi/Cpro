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
#include <time.h>

typedef struct subject{
    int spectrum;
    int total_students;
    int student_no[4];
    int sema_val;
}subject;

typedef struct pupil{
    int choices[8];
    int student_no;
    int allocated[4];
    int type;
}pupil;

// student can be of 4 types : 0-3
// global declarations {{{
sem_t semaphore;
sem_t hold[3];
subject *course;
pupil *student;
int thrd_cnt;
int totCourse;
int totStudents;
int tmp_totStudents;
int SIZE_PREF = 8;
// }}}

void initalise(int type, int val, int index)
{
    if (type==0)
    {
        course[index].spectrum = val;
        course[index].student_no[0]  = 12;
        course[index].student_no[1]  = 12;
        course[index].student_no[2]  = 24;
        course[index].student_no[3]  = 12;
        course[index].total_students = 0;
        //course[index].subject =  ;
    }
    else if (type==1)
    {
        student[index].type = rand()%4;
        student[index].student_no = index;

        int i=0, spectrum[]={1,1,1,1};
        for (i = 0; i < SIZE_PREF; )
        {
            int r = rand()%val;
            int s_no = course[r].spectrum;
            if (spectrum[s_no]<=0 && (spectrum[(s_no+1)%4]>0 || //
                        spectrum[(s_no+2)%4]>0 || spectrum[(s_no+3)%4]>0 ))
                continue;

            int flag=0, j=0;
            for (j = 0; j < i; ++j)
            {
                if (student[index].choices[j]==r)
                    flag = 1;
            }
            if (flag!=1)
            {
                student[index].choices[i]=r;
                spectrum[s_no] -= 1;
                i+=1;
            }
        }
        for (i = 0; i < 4; ++i)
            student[index].allocated[i] = -1;
    }
    return;
}

void write_file()
{
    FILE *fd = fopen("allocation.txt", "w");
    int i;
    for (i=0; i<totStudents; i++)
    {
        int *arr = student[i].allocated;
        if (arr[0]!=-1 && arr[1]!=-1 && arr[2]!=-1 && arr[3]!=-1 )
            /*printf("Student %d : %d %d %d %d\n", i, arr[0], arr[1], arr[2], arr[3] );*/
            fprintf(fd, "Student %d : %d %d %d %d\n", i, arr[0], arr[1], arr[2], arr[3] );
    }
    fclose(fd);
}

int check_spectrum(int index, int spectrum)
{
    int i;
    for (i = 0; i < 4; ++i)
    {
        if (student[index].allocated[i]==spectrum)
            return -1;
    }
    return 1;
}

int select_course(int index)
{
    int i=0, j=0, course_no;
    int type = student[index].type;
    while (j != 4)
    {
        course_no = student[index].choices[i];
        if (course_no!= -1 && course[course_no].student_no[type]>0)
        {
            int spectrum = course[course_no].spectrum;
            int allocate = check_spectrum(index, spectrum);
            if (allocate==1)
            {
                course[course_no].student_no[type] -= 1;
                student[index].allocated[j]         = course_no;
                student[index].choices[i]           = -1;
                j++;
            }
        }
        if(i==SIZE_PREF-1)
            break;
        i++;
    }
    tmp_totStudents -= 1;
    if (j==4)
        return 1;
    else
        return -1;
}

void * allocation(void * arg)
{
    pupil *student = (pupil *) arg;
    int flag=-1;
    /*while (flag==-1 && tmp_totStudents>0)*/
    /*{*/
        sem_wait(&hold[0]);
            flag = select_course(student->student_no);
            //printf("%d  %d\n", flag, tmp_totStudents);
        sem_post(&hold[0]);
    /*}*/
    if (flag==-1)
    {
        sem_wait(&hold[1]);
            printf("Student %d Starved\n", student->student_no);
        sem_post(&hold[1]);
    }

    sem_wait(&hold[2]);
        thrd_cnt -= 1;
        /*printf("[%d]", thrd_cnt );*/
    sem_post(&hold[2]);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    // variable declarations {{{
    totStudents     = atoi(argv[1]);
    thrd_cnt        = atoi(argv[1]);
    tmp_totStudents = atoi(argv[1]);
    totCourse       = atoi(argv[2]);
    int err, i=0, j=0;
    /*thrd_cnt  = 0;*/

    // initialse group
    course  = (subject *)malloc(sizeof(subject)*totCourse);
    student = (pupil *)malloc(sizeof(pupil)*totStudents);
    for (i = 0; i < totCourse; ++i)
        initalise(0, i%4, i);
    for (i = 0; i < totStudents; ++i)
        initalise(1, totCourse, i);

    sem_init(&hold[0], 0, 1);
    sem_init(&hold[1], 0, 1);
    sem_init(&hold[2], 0, 1);
    // create threads out of atoms
    pthread_t tid;
    // }}} 

    for (i = 0; i < totStudents; i++)
    {
        //int courseNo = select_course(i);
        //thrd_cnt += 1;
        err = pthread_create(&tid, NULL, allocation, &student[i]);

        if (err!=0)
        {
            i-= 1;
            /*thrd_cnt -= 1;*/
            printf("***Error creating thread\n");
        }

        /*printf("Student No : %d , %d\n", i, student[i].type);*/
        /*for (j = 0; j < 8; ++j)*/
        /*{*/
            /*printf("[%d,%d] ", student[i].choices[j], course[student[i].choices[j]].spectrum );*/
        /*}*/
        /*printf("\n");*/
    }
    //wait for all thread to terminate
    while(thrd_cnt>0);
    printf("\n[+] Writting course allocation list to file\n\n");
    write_file();
    printf("Terminating...\n");

    return 0;
}
