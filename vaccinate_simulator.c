#include <sys/stat.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>

typedef struct thread_parameters{
    char name[100];
    int quality;
    int speed;
    int cost;
    int id;
    sem_t * semaphore;
}thread_parameters;

int fd1=0;
int money=0;
int lowest_student_cost=999999;
int * busy_student_list;
sem_t queue_semaphore;
sem_t student_solving_semaphore;
sem_t * student_semaphore;
sem_t print_semaphore;
sem_t lock;
int h_done=0;
int m_done=0;

void * H_threadFunc(void * variables);
void * Student_for_hire_threadFunc(void * variables);
void push_queue(char * queue,char c);
char pop_queue(char * queue);
void SIGINT_handler(int signa);
void SIGINT_handler2(int signa);

int main(int argc, char ** argv){

    int fd2=0;
    int i=0;
    char printformat[10000];

    if(argc!=4){
        fprintf(stderr,"Too many/less arguments.\n");
        exit(EXIT_FAILURE);
    }
    fd1=open(argv[1],O_RDONLY);
    fd2=open(argv[2],O_RDONLY);
    for(i=0;i<strlen(argv[3]);i++){
        if(argv[3][i]<'0' || argv[3][i]>'9'){
            fprintf(stderr,"Invalid money.\n");
            exit(EXIT_FAILURE);
        }
    }
    sscanf(argv[3],"%d",&money);
    if(money<0){
        fprintf(stderr,"Invalid money.\n");
        exit(EXIT_FAILURE);
    }

    if(fd1<0 || fd2<0){
        perror("Open error");
    }

    signal(SIGINT,SIGINT_handler2);
    char c;
    int line_size=0;
    int done=0;
    int rd=0;
    i=0;

    do{
        rd=read(fd2,&c,1);
        if(rd>0){
            if(c=='\n'){
                line_size++;
            }
        }
        else if(rd<0){
            perror("Read Error");
        }
    }
    while(rd!=0);
    if(c!='\n'){
        line_size++;
    }
    lseek(fd2,0,SEEK_SET);
    busy_student_list=(int *)malloc(sizeof(int)*line_size);
    char * queue=(char *)malloc(sizeof(char)*money);
    queue[0]='\0';
    student_semaphore=malloc(sizeof(sem_t)*line_size);

    signal(SIGINT,SIGINT_handler);
    int money_made[line_size];
    char temp[line_size][1000];
    int line=0;
    do{
        i=0;
        do{
            rd=read(fd2,&c,1);
            if(rd>0){
                temp[line][i]=c;
                i++;
            }
            else if(rd<0){
                perror("Read Error");
            }
            else{
                done=1;
                break;
            }   
        }
        while(c!='\n');
        busy_student_list[line]=0;
        money_made[line]=0;
        line++;
    }
    while(done!=1);

    pthread_t H_thread;
    pthread_t Student_for_hire_thread[line_size];
    thread_parameters param[line_size];
    
    sem_init(&queue_semaphore,0,0);
    sem_init(&print_semaphore,0,1);
    sem_init(&lock,0,1);
    sem_init(&student_solving_semaphore,0,line_size);

    for(i=0;i<line_size;i++){
        sscanf(temp[i],"%s %d %d %d",param[i].name,&param[i].quality,&param[i].speed,&param[i].cost);
        sem_init(&student_semaphore[i],0,0);
        param[i].id=i;
        param[i].semaphore=&student_semaphore[i];
    }
    
    for(i=0;i<line_size;i++){
        if(param[i].cost<lowest_student_cost){
            lowest_student_cost=param[i].cost;
        }
    }
    if(pthread_create(&H_thread,NULL,H_threadFunc,queue)<0){
        perror("pthread_create error");
        exit(EXIT_FAILURE);
    }

    for(i=0; i<line_size;i++){
        if(pthread_create(&Student_for_hire_thread[i],NULL,Student_for_hire_threadFunc,&param[i])<0){
            perror("pthread_create error");
            exit(EXIT_FAILURE);
        }
    }

    do{
        char curr;
        int chosen=-1;
        int highest=0;
        int lowest_cost=999999;
        sem_wait(&student_solving_semaphore);
        sem_wait(&queue_semaphore);
        curr=pop_queue(queue);
        sem_wait(&lock);
        if(curr=='Q'){
            for(i=0;i<line_size;i++){
                if(busy_student_list[i]==0 && param[i].quality>highest && money>param[i].cost){
                    chosen=i;
                    highest=param[i].quality;
                }
            }
        }
        else if(curr=='S'){
            for(i=0;i<line_size;i++){
                if(busy_student_list[i]==0 && param[i].speed>highest && money>param[i].cost){
                    chosen=i;
                    highest=param[i].speed;
                }
            } 
        }
        else if(curr=='C'){
            for(i=0;i<line_size;i++){
                if(busy_student_list[i]==0 && param[i].cost<lowest_cost && money>param[i].cost){
                    chosen=i;
                    lowest_cost=param[i].cost;
                }
            }
        }
        sem_post(&lock);
        if(chosen!=-1){
            money-=param[chosen].cost;
            sem_wait(&print_semaphore);
            sprintf(printformat,"%s is chosen for solving homework %c.Remaining money is %dTL\n",param[chosen].name,curr,money);
            write(1,printformat,strlen(printformat));
            sem_post(&print_semaphore);
            busy_student_list[chosen]=1;
            money_made[chosen]++;        
            sem_post(&student_semaphore[chosen]);
            
        }
    }
    while((h_done!=1 || strlen(queue)!=0) && (money>=lowest_student_cost));
    m_done=1;
    for(i=0;i<line_size;i++){
        sem_post(&student_semaphore[i]);
    }

    if(pthread_join(H_thread,NULL)<0){
        perror("pthread_join error");
        return -1;
    }

    for(i=0; i<line_size;i++){
        if(pthread_join(Student_for_hire_thread[i],NULL)<0){
            perror("pthread_join error");
            return -1;
        }
    }

    if(money<lowest_student_cost){
        write(1,"Money is over, closing.\n",strlen("Money is over, closing.\n"));
    }
    else if(strlen(queue)==0){
        write(1,"No more homeworks left or coming in, closing.\n",strlen("No more homeworks left or coming in, closing.\n"));
    }

    write(1,"Homeworks solved and money made by the students:\n",strlen("Homeworks solved and money made by the students:\n"));
    
    int total_cost=0;
    int total_homeworks=0;
    for(i=0; i<line_size;i++){
        sprintf(printformat,"%s %d %dTL\n",param[i].name,money_made[i],money_made[i]*param[i].cost);
        write(1,printformat,strlen(printformat));
        total_cost+=money_made[i]*param[i].cost;
        total_homeworks+=money_made[i];
    }
    sprintf(printformat,"Total cost for %d homeworks %dTL\n",total_homeworks,total_cost);
    write(1,printformat,strlen(printformat));
    sprintf(printformat,"Money left at H's account %dTL\n",money);
    write(1,printformat,strlen(printformat));

    sem_destroy(&queue_semaphore);
    sem_destroy(&print_semaphore);
    sem_destroy(&lock);
    sem_destroy(&student_solving_semaphore);
    free(queue);
    free(busy_student_list);
    free(student_semaphore);
    close(fd1);
    close(fd2);
    return 0;
}

void * H_threadFunc(void * variables){
    char * queue = variables;
    char c;
    char printformat[10000];
    int size;
    int rd=0;
    do{
        rd=read(fd1,&c,1);
        if(rd>0 && (c=='S' || c=='Q' || c=='C')){
            push_queue(queue,c);
            sem_wait(&print_semaphore);
            size=sprintf(printformat,"H has a new homework %c; remaining money is %dTL\n",c,money);
            write(1,printformat,size);
            sem_post(&print_semaphore);
            sem_post(&queue_semaphore);
        }
        else if(rd<0){
            perror("Read Error");
        }
    }
    while(rd!=0 && money>=lowest_student_cost);
    h_done=1;
    if(money<lowest_student_cost){
    write(1,"H has no more money for homeworks,terminating.\n",strlen("H has no more money for homeworks,terminating.\n"));
    }
    else{
    write(1,"H has no other homeworks,terminating.\n",strlen("H has no other homeworks,terminating.\n"));
    }
    return 0;
}
void * Student_for_hire_threadFunc(void * variables){
    thread_parameters * param = variables;

    char printformat[10000];
    int size;

    do{
        sem_wait(&print_semaphore);
        size=sprintf(printformat,"%s is waiting for a homework\n",param->name);
        write(1,printformat,size);
        sem_post(&print_semaphore);
        sem_wait(param->semaphore);
        if(m_done==1 && busy_student_list[param->id]==0){
            break;
        }
        sem_wait(&print_semaphore);
        size=sprintf(printformat,"%s is solving homework in %d seconds.\n",param->name,6-param->speed);
        write(1,printformat,size);
        sem_post(&print_semaphore);
        usleep((6-param->speed)*1000000);
        sem_wait(&lock);
        busy_student_list[param->id]=0;
        sem_post(&lock);
        sem_post(&student_solving_semaphore);
    }
    while(m_done!=1);
    sem_wait(&print_semaphore);
    size=sprintf(printformat,"%s is done, terminating.\n",param->name);
    write(1,printformat,size);
    sem_post(&print_semaphore);
    return 0;
}

void push_queue(char * queue,char c){
    int i=0;
    while(queue[i]!='\0'){
        i++;
    }
    queue[i]=c;
    queue[i+1]='\0';
}
char pop_queue(char * queue){
    char ret=queue[0];
    int i=0;
    while(queue[i]!='\0'){
        queue[i]=queue[i+1];
        i++;
    }
    return ret;
}

void SIGINT_handler(int signa){
    //free(queue_pt);
    //free(busy_student_list);
    //free(student_semaphore);
    write(1,"SIGINT received.All resources are freed.\n",strlen("SIGINT received.All resources are freed.\n"));
    exit(EXIT_FAILURE);
}

void SIGINT_handler2(int signa){
    write(1,"SIGINT received.All resources are freed.\n",strlen("SIGINT received.All resources are freed.\n"));
    exit(EXIT_FAILURE);
}
