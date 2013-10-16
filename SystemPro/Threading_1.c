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

typedef struct rxn_site{
    int atoms_entered;
    int hatoms;
    int oatoms;
    int site_no;
    int intake;
    int exit;
    int sema_val;
    int energy;
    sem_t semaphore;
}rxn_site;

typedef struct atm{
    int type;
}atm;

// global declarations {{{
sem_t semaphore[3];
int thresholdE;
rxn_site *sites;
atm *atoms;
int tot_sites;
int h_atoms;
int o_atoms;
int thrd_cnt;
int totalE;
// }}}

void initalise_sites(int tot_sites)
{
    int i=0;
    for (i = 0; i < tot_sites; ++i){
        sites[i].atoms_entered = 0;
        sites[i].hatoms        = 2;
        sites[i].oatoms        = 1;
        sites[i].site_no       = i;
        sites[i].intake        = 1;
        sites[i].exit          = 0;
        sites[i].energy        = 0;
        sem_init(&sites[i].semaphore, 0, 3 );
    }
    return;
}

void initalise_atoms(int tot_atoms)
{
    int i;
    for (i=0; i<tot_atoms; i++)
    {
        int r = rand()%2;
        if(r==0 && h_atoms>0)
        {
            atoms[i].type = 0;
            h_atoms -= 1;
        }
        else if (r==1 && o_atoms>0)
        {
            atoms[i].type = 1;
            o_atoms -= 1;
        }
        else
            i-=1;
    }
}

int check_sites(int no)
{
    int flag_prev = 0, flag_aftr=0;
    if ( no>0 && sites[no].intake==1 && sites[no-1].atoms_entered==0 ) {
        flag_prev=1;
    }
    else if ( sites[no].intake==1 && no==0 ) flag_prev=1;

    if ( no<tot_sites-1 && sites[no].intake==1 && sites[no+1].atoms_entered==0 ) {
        flag_aftr=1;
    }
    else if ( sites[no].intake==1 && no==tot_sites-1) flag_aftr=1;

    if (flag_prev==1 && flag_aftr==1)
        return 1;
    else
        return 0;
} 

void print_rxn_info()
{
    int i;
    printf("Sites :\t");
    for (i = 0; i < tot_sites; ++i)
    {
        printf("%d,%d  ", 2-sites[i].hatoms, 1-sites[i].oatoms );
    }
    printf("\nT.E.  : %d\n", totalE);
}

int cal_energy()
{
    int i; totalE=0;
    for (i = 0; i < tot_sites; ++i)
        totalE += sites[i].energy;
    return totalE;
}

int select_site(int type)
{
    int i, atoms_no=-1, selected_site=-1;
    for(i=0; i<tot_sites; i++)
    {
        int go = check_sites(i);
        if (go==0) continue;
        if ( type==0 && sites[i].hatoms>0 && sites[i].atoms_entered > atoms_no)
        {
            selected_site = i;
            atoms_no = sites[i].atoms_entered;
        }
        if (type==1 && sites[i].oatoms>0 && sites[i].atoms_entered > atoms_no)
        {
            selected_site = i;
            atoms_no = sites[i].atoms_entered;
        }
    }
    if (selected_site!=-1)
    {
        if (type==0)
        {
            sites[selected_site].hatoms -= 1;
            sites[selected_site].atoms_entered += 1;
            h_atoms -= 1;
            return selected_site;
        }
        if (type==1)
        {
            sites[selected_site].oatoms -= 1;
            sites[selected_site].atoms_entered += 1;
            o_atoms -= 1;
            return selected_site;
        }
    }
    return -1;
}

void * reaction(void * arg)
{
    atm *atom = (atm *) arg;
    int value, site_no=-1;
    while(site_no==-1)
    {
        sem_wait(&semaphore[0]);
            totalE = cal_energy();
            if (totalE < thresholdE)
            {
                site_no = select_site(atom->type);
                if ( site_no != -1 && sites[site_no].atoms_entered==3)
                    sites[site_no].intake=0;
            }
            else
                site_no = -1;
        sem_post(&semaphore[0]);
    }

    sem_wait(&semaphore[1]);
        print_rxn_info();
    sem_post(&semaphore[1]);

    sem_wait(&sites[site_no].semaphore);

    sem_getvalue(&sites[site_no].semaphore, &sites[site_no].sema_val );
    // wait for other atoms to come
    while(sites[site_no].atoms_entered!=3 && sites[site_no].sema_val != 0);
    sites[site_no].energy = 1;

    sem_post(&sites[site_no].semaphore);

    // thread count check
    sem_wait(&semaphore[2]);
        sites[site_no].exit += 1;
        thrd_cnt -= 1;

        if (sites[site_no].exit == 3 )
        {
            /*print_rxn_info();*/
            printf("Remaing atoms: %d %d\n", h_atoms, o_atoms);
            printf("***RXN END\n");
            sites[site_no].energy = 0;
            sites[site_no].hatoms = 2;
            sites[site_no].oatoms = 1;
            sites[site_no].atoms_entered = 0;
            sites[site_no].exit = 0;
            sites[site_no].intake = 1;
        }
    sem_post(&semaphore[2]);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
    srand(time(NULL));
    // variable declarations {{{
    int tot_atoms = atoi(argv[1]) + atoi(argv[2]);
    h_atoms = atoi(argv[1]);
    o_atoms = atoi(argv[2]);
    int err, i=0, j=0;
    tot_sites = atoi(argv[3]);
    thrd_cnt  = 0;

    // initialse rxn sites
    sites = (rxn_site *)malloc(sizeof(rxn_site)*tot_sites);
    atoms = (atm *)malloc(sizeof(atm)*tot_atoms);
    initalise_sites(tot_sites);
    initalise_atoms(tot_atoms);
    h_atoms = atoi(argv[1]);
    o_atoms = atoi(argv[2]);
    
    totalE = 0;
    thresholdE = atoi(argv[4]);
    
    // create threads out of atoms
    pthread_t tid;

    sem_init(&semaphore[0], 0, 1 );
    sem_init(&semaphore[1], 0, 1 );
    sem_init(&semaphore[2], 0, 1 );
    /// }}}
    for (i = 0; i < tot_atoms; i++) {
        // check sites for consective reaction
        thrd_cnt += 1;
        err = pthread_create(&tid, NULL, reaction, &atoms[i] );
        // check if the thread creation fails
        if (err!=0)
        {
            thrd_cnt -= 1;
            i-=1;
            printf("*** Error : Thread creation failed\n");
        }
    }
    //wait for all thread to terminate
    while(thrd_cnt!=0);
    printf("Terminating...\n");
    return 0;
}
