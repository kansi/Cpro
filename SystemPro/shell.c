#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

typedef struct shell {
    char *argv[64];
    int pipe;
    int inpRdct;
    int outRdct;
    int bg;
    char shellConst;
    struct shell *next;

} shell;

typedef struct job {
    struct job *next;           /* next active job */
    pid_t pid;                 /* process group ID */
    char argv[64][64];
    int jobNum;
    int cnt;
    //char notified;              /* true if user told about stopped job */
    //char completed;             /* true if process has completed */
    //char stopped;               /* true if process has stopped */
    int status;                 /* reported status value */
} job;  

pid_t shell_pgid;
pid_t curr_fg_pid;
struct termios shell_tmodes;
struct termios process_tmodes;

int shell_terminal;
int shell_is_interactive;
int chk_ctrlZ;

job *bgJobs = NULL;
job *iter   = NULL;
int jobNo;
/* Make sure the shell is running interactively as the foreground job
   before proceeding. */
void pHandle(int sig);

void init_shell () {
    /* See if we are running interactively.  */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty (shell_terminal);

    if (shell_is_interactive)
    {
        /* Ignore interactive and job-control signals.  */
        signal (SIGINT, SIG_IGN);
        signal (SIGQUIT, SIG_IGN);
        signal (SIGTSTP, SIG_IGN);
        signal (SIGTTIN, SIG_IGN);
        signal (SIGTTOU, SIG_IGN);
        //signal (SIGCHLD, SIG_IGN);

        /* Put ourselves in our own process group.  */
        shell_pgid = getpid ();
        //printf("%d\n", shell_pgid);
        if (setpgid (shell_pgid, shell_pgid) < 0)
        {
            perror ("Couldn't put the shell in its own process group");
            exit (1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp (shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr (shell_terminal, &shell_tmodes);
    }
}  

void parse2(char *line, char **argv) {
    char *cmd;
    cmd = strtok(line," \t");

    while (cmd != NULL) {
        *argv++ = cmd;
        cmd = strtok(NULL, " \t");
    }
    *argv = '\0';
}

// display the ps1 for the shell
void ps1(){

    char *pwd  = getenv("PWD");
    char *cwd  = getcwd(0, 0);

    if ( strncmp(cwd, pwd, strlen(pwd)) == 0 ){
        char ps1[ strlen(cwd) - strlen(pwd) ];
        strcpy(ps1, &cwd[strlen(pwd)]);
        printf("[%s@%s ~%s]$ ", getenv("USER"),getenv("HOSTNAME"), ps1);
    }
    else{
        printf("[%s@%s %s]$ ", getenv("USER"),getenv("HOSTNAME"), cwd);
    }
    return ;
}

int bg(char **argv) {

    while(*argv != NULL ){
        //printf("%s\n", *argv);
        if (strcmp(*argv, "&") == 0) {
            *argv = NULL;
            return 1;
        }
        *argv++;
    }
    return 0;
}

void handler(int sig){
    if(sig == SIGTSTP) {
        signal(sig, SIG_IGN);
        kill(shell_pgid, SIGTSTP);
        //dup2(stdin,STDOUT_FILENO);
        //dup2(stdout,STDERR_FILENO);
 
    }
    // restore signal handler
    signal (SIGTSTP, handler);
    return;
}

// forks a child and executes the command
void execute(shell *command) {
    char **argv = command->argv;
    pid_t  pid;
    pid_t  ppid = getpid();
    pid_t  pgid = getpgrp();
    int    status;

    // check is the requested command is bg process
    int bgProcess = bg(argv);

    if ((pid = fork()) < 0) {
        perror("*** ERROR: forking child process failed\n");
        exit(1);
    }
    else{
        curr_fg_pid = pid;
        if (pid == 0) {
            if (bgProcess){
                setpgid(0,0);
                //close(STDOUT_FILENO);
                // redirect the output and error to null
                //int devNull = open("/dev/null", O_WRONLY);
                //dup2(devNull,STDOUT_FILENO);
                //dup2(devNull,STDERR_FILENO);
            }
            /* Set the handling for job control signals back to the default.  */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            //signal(SIGTCONT, handler);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            if (execvp(*argv, argv) < 0) {
                perror("*** ERROR: command execution failed");
                exit(1);
            }
        }
        else {
            //storeJob( bgJobs, pid, argv);
            bgJobs->pid = pid;
            bgJobs->jobNum = ++jobNo;
            // save the command with args
            int i=0;
            //printf("%s\n", argv[0]);
            while(argv[i]!=0){
                sprintf(bgJobs->argv[i], "%s", argv[i]);
                i++;
            }
            bgJobs->cnt = i;
            bgJobs->next = malloc(sizeof(job));
            bgJobs = bgJobs->next;
            bgJobs->next = NULL;

            // wait if the the procees is foreground
            if(!bgProcess) {

                while(waitpid( 0, &status, WUNTRACED )!=pid);
                //printf("%d\n", status);
                //remove the the job from job pool if it has finshed its exec
                if(status==0){
                    job *tmp = iter;
                    job *prev = NULL;
                    while (iter->next!=NULL){
                        //printf("%d %s\n", iter->pid, iter->argv[0]);
                        if(iter->pid == pid){
                            jobNo -= 1;
                            if(prev!=NULL){
                                prev->next = iter->next;
                                iter->next = NULL;
                            }
                            else{
                                tmp = iter->next;

                                iter->next = NULL;
                            }
                            break;
                        }
                        prev = iter;
                        iter = iter->next;
                    }
                    iter = tmp;
                }

            }
            else{
                kill(ppid, SIGTSTP);
                //kill(ppid, SIGCHLD);
            }
        }
    }
}

int shellConst(char *cmd, shell *command){

    if (strcmp(cmd, "&")==0){
        command->bg=0;
        return 0;
    }
    else if (strcmp(cmd, "<")==0){
        command->inpRdct=1;
        command->shellConst=cmd[0];
        return 1;
    }
    else if (strcmp(cmd, "|")==0){
        command->pipe+=1;
        command->shellConst=cmd[0];
        return 1;
    }
    else if (strcmp(cmd, ">")==0){
        command->outRdct+=1;
        command->shellConst=cmd[0];
        return 1;
    }
    return 0;
}

void parse(char *line, shell *command) {
    char *cmd;
    char **argv = command->argv;
    cmd = strtok(line," \t");

    while (cmd != NULL) {

        if( shellConst(cmd, command) ){
            *argv = '\0';
            shell *newcmd = malloc(sizeof(shell));
            command->next = newcmd;
            command       = command->next;
            argv          = command->argv;
            cmd           = strtok(NULL, " \t");
        }
        *argv = cmd;
        //printf("%s\n", *argv);
        cmd = strtok(NULL, " \t");
        *argv++;
    }
    *argv = '\0';
    command->next = NULL;
}

int customCmd(char **argv){
    if(strcmp( argv[0], "cd")==0)
        return 1;
    else if(strcmp( argv[0], "pinfo")==0)
        return 2;
    else if(strcmp( argv[0], "kjob")==0)
        return 3;
    else if(strcmp( argv[0], "overkill")==0)
        return 4;
    else if(strcmp( argv[0], "fg")==0)
        return 5;
    else if(strcmp( argv[0], "jobs")==0)
        return 6;

    return 0;

}

void killJob(shell *command){
    job *tmp = iter;
    job *prev = NULL;
    int flag=0;
    while (iter->next!=NULL) {
        if(iter->jobNum == atoi(command->argv[1])){
            flag = 1;
            kill(iter->pid, atoi(command->argv[2]));
            if(prev!=NULL){
                prev->next = iter->next;
                iter->next = NULL;
            }
            else{
                tmp = tmp->next;
            }
        }
        prev = iter;
        iter = iter->next;
    }
    if(flag==0)
        printf("***ERROR: No job with job number %s\n", command->argv[1] );
    iter = tmp;
    return;
}

void overkill(shell *command) {
    job *tmp=NULL;
    while (iter->next!=NULL) {
        kill(iter->pid, 9 );
        tmp = iter;
        iter = iter->next;
        tmp->next = NULL;
    }
    return;
}

void fg(shell *command){
    job *tmp = iter;
    job *prev = NULL;
    int status;
    int flag=0;
    while (iter->next!=NULL) {

        if(  iter->jobNum == atoi(command->argv[1]) ){
            flag=1;
            // bring the background process to the foreground
            tcsetpgrp(shell_terminal, iter->pid);

            // send contiue signal
            kill ( iter->pid, SIGCONT );
            //set the current fg process
            curr_fg_pid = iter->pid;

            // wait for its termination
            waitpid(-iter->pid, &status, WUNTRACED);

            // remove the process from the job pool if done
            if(status==0){
                jobNo -= 1;
                if(prev!=NULL){
                    prev->next = iter->next;
                    iter->next = NULL;
                }
                else
                    tmp = tmp->next;
            }
            // bring terminal back in fg
            pid_t pgid = getpid();
            tcsetpgrp (shell_terminal, shell_pgid);
            break;
        }
        prev = iter;
        iter = iter->next;
    }
    if(flag==0)
        printf("***ERROR: No job exists for job number %s", command->argv[1]);
    iter = tmp;
    return ;
}

void jobs(){
    job *tmp = iter;
    job *prev = NULL;
    while (iter->next!=NULL){
        printf("[%d] ", iter->jobNum);
        int i=0;
        while(i!= iter->cnt) {
            printf("%s ", iter->argv[0]);
            i++;
        }
        printf("[%d]\n", iter->pid);
        prev = iter;
        iter = iter->next;
    }
    iter = tmp;
    return;

}

void pinfo(shell *command){
    int size;
    char pid[10];
    if ( sizeof(command->argv[1])==0 )
        size = 13+sizeof(command->argv[1]);
    else
        size = 13+sizeof( itoa(getpid()) );

    char path[size];

    if (sizeof(command->argv[1])==0)
        sprintf(path, "/proc/%s/status", command->argv[1] );
    else
        sprintf(path, "/proc/%d/status", getpid() );

    FILE *fd;
    fd = fopen( path, "r");
    int i = 1;
    char c;
    while (i!=13){
        c = getc(fd);
        if (i==1 || i==2 || i==12){
            printf("%c", c);
        }
        if (c=='\n'){
            //printf("%d\n",i);
            i++;
        }
    }
    //printf("\n");
    fclose(fd);
}

void customExec(shell *command, int i){
    char **argv=command->argv;

    switch(i){
        case 1:
            chdir(command->argv[1]);
            break;
        case 2:
            pinfo(command);
            break;
        case 3:
            killJob(command);
            break;
        case 4:
            overkill(command);
            break;
        case 5:
            fg(command);
            break;
        case 6:
            jobs(command);
            break;

    }
    return;
}

void preProcess(shell *command){
    do{
        /*char **print = command->argv;*/
        /*while(*print != '\0'){*/
            /*//printf("%s", *print);*/
            /**print++;*/
        /*}*/
        int cmdNo = customCmd(command->argv);

        if ( cmdNo > 0 ){
            customExec(command, cmdNo);
        }
        else{
            execute(command);
        }
        command = command->next;
        //printf("\n");
    }while(command != NULL);

    // create a while loop that loops through all commands
    return ;
}

void pHandle(int sig){

    if(sig == SIGTSTP) {
        signal(sig, SIG_IGN);

        setpgid( curr_fg_pid, 0 );
        pid_t pgid = getpid();
        //kill ( curr_fg_pid, SIGTSTP );
        tcsetpgrp (shell_terminal, pgid);
        printf("[+] Process %d stopped\n", curr_fg_pid);
    }
    // restore signal handler
    signal (SIGTSTP, pHandle);
    return;
}

void chkStatus(){
    job *tmp = iter;
    job *prev = NULL;
    int status;
    while (iter->next!=NULL){
        //printf("%d\n", iter->pid);
        int state = waitpid(iter->pid, &status, WNOHANG | WUNTRACED);
        if(status==0){
            printf("[+] %s with pid %d exited with status %d\n", iter->argv[0], iter->pid, status);
            jobNo -= 1;
            if(prev!=NULL){
                prev->next = iter->next;
                iter->next = NULL;
            }
            else
                tmp = tmp->next;
            break;
        }
        prev = iter;
        iter = iter->next;
    }
    iter = tmp;
    return ;
}

int main()  {
    init_shell();

    jobNo = 0;
    bgJobs = malloc(sizeof(job));
    bgJobs->next = NULL;
    iter = bgJobs;

    chk_ctrlZ=0;

    char  line[1024];
    int   status;
    shell *command = malloc(sizeof(shell));
    shell *rootcmd = command;

    signal (SIGTSTP, pHandle);
    //signal (SIGCHLD, pHandle);

    while (1) {
        ps1();
        gets(line);
        if(strlen(line)) {

            //parse2(line, command->argv);
            parse(line, command);
            command = rootcmd;
            if (strcmp(command->argv[0], "quit") == 0){
                //tcsetattr (shell_terminal, TCSANOW, &shell_tmodes);
                bgJobs = NULL;
                iter = NULL;
                exit(0);
            }
            preProcess(command);

            job *tmp = iter;
            int status;
            while (iter->next!=NULL) {
                //printf("%d %s\n", iter->pid, iter->argv[0]);
                iter = iter->next;
            }
            iter = tmp;
        }
        //chkStatus();
        /*if ( waitpid(-1, &status, WNOHANG) > 0) {*/
            /*printf("Process exited with status %d\n", status);*/
        /*}*/
    }
    return 0;
}
