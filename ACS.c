#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define num_clerks 2
#define num_queues 4

int num_cus;
double total_wait_time = 0;
struct cus *customers = NULL;
int clerks[num_clerks];
struct cus *queue0 = NULL;
struct cus *queue1 = NULL;
struct cus *queue2 = NULL;
struct cus *queue3 = NULL;
int q_size[num_queues];
pthread_mutex_t queues_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clerks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t total_wait_time_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t q0_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t q1_cond = PTHREAD_COND_INITIALIZER; 
pthread_cond_t q2_cond = PTHREAD_COND_INITIALIZER; 
pthread_cond_t q3_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t c0_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t c1_cond = PTHREAD_COND_INITIALIZER;

struct cus {
    int cus_id;
    double sleep_time;
    double serv_time;
    double begin_wait_time;
    struct cus *next;
};

void printList(struct cus *list) {
    struct cus *ptr = list;
    while(ptr != NULL) {
        printf("%d: %f %f\n", ptr->cus_id, ptr->sleep_time, ptr->serv_time);
        ptr = ptr->next;
    }
}

int size(struct cus *list) {
    struct cus *ptr = list;
    int size = 0;
    while(ptr != NULL) {
        ++size;
        ptr = ptr->next;
    }
    return size;
}

struct cus *addCus(struct cus *list, int cus_id, double sleep_time, double serv_time, time_t begin_wait_time) {
    struct cus *ptr = (struct cus*) malloc(sizeof(struct cus));
    struct cus *temp = list;
    ptr->cus_id = cus_id;
    ptr->sleep_time = sleep_time;
    ptr->serv_time = serv_time;
    ptr->begin_wait_time = begin_wait_time;

    if(list == NULL) list = ptr;
    else {
        while(temp->next != NULL) temp = temp->next;
        temp->next = ptr;
    }
    return list;
}

struct cus *removeCus(struct cus *list) {
    printf("REMOVAL: Customer %d removed from queue\n", list->cus_id);    
    if(list == NULL) return NULL;
    list = list->next; 
    return list;
}

int pickQueue(int shortest) {
    int pick = 0;

    if(shortest == 0) {
        if((q_size[0] == 0) && (q_size[1] == 0) && (q_size[2] == 0) && (q_size[3] == 0)){
            printf("QUEUE: No One Waiting\n");
            return -1;
        }
    }
    
    if((q_size[0] == q_size[1]) && (q_size[1] == q_size[2]) && (q_size[2] == q_size[3])){
        pick = rand() % 4;
        return pick;
    }

    int i;
    if (shortest == 1) {
        for(i = 0; i < 4; i++) {
            if (q_size[pick] > q_size[i]) pick = i;
            else if (q_size[pick] == q_size[i]) {
                if (rand() % 2 == 1) pick = i;
            }
        }
    } else {
        for(i = 0; i < 4; i++) {
            if (q_size[pick] < q_size[i]) pick = i;
            else if (q_size[pick] == q_size[i]) {
                if (rand() % 2 == 1) pick = i;
            }
        }
    }
    return pick;
}

int clerkAvail(int q_num) {
    int i;
    for (i = 0; i < num_clerks; i++) {
        if (clerks[i] == q_num) {
            clerks[i] = -1;
            return i;
        }
    }
    return -1;
}

void printAll() {
    printf("LOG: Queue 0\n");
    printList(queue0);
    printf("LOG: Queue 1\n");
    printList(queue1);
    printf("LOG: Queue 2\n");
    printList(queue2);
    printf("LOG: Queue 3\n");
    printList(queue3);
    fflush(stdout);
    printf("LOG: Queue 0 size: %d\n", q_size[0]);
    printf("LOG: Queue 1 size: %d\n", q_size[1]);
    printf("LOG: Queue 2 size: %d\n", q_size[2]);
    printf("LOG: Queue 3 size: %d\n", q_size[3]);
    fflush(stdout);
}

void *cus_thread(void *cus) {
    struct cus *cus_info = cus;
    int serving_clerk;
    usleep(cus_info->sleep_time * 1000000);
    printf("ARRIVAL: Customer %d has arrived after %f\n", cus_info->cus_id, cus_info->sleep_time);

    pthread_mutex_lock(&queues_mutex);
    int shortest = pickQueue(1);
    if (shortest == 0)  {
        queue0 = addCus(queue0, cus_info->cus_id, cus_info->sleep_time, cus_info->serv_time, time(NULL));
        q_size[0]++;
        printAll();
        printf("ARRIVAL: Added customer %d to queue 0\n", cus_info->cus_id);
        while(1) {
            pthread_cond_wait(&q0_cond, &queues_mutex);
            if (cus_info->cus_id == queue0->cus_id) {
                pthread_mutex_lock(&clerks_mutex);
                serving_clerk = clerkAvail(0);
                pthread_mutex_unlock(&clerks_mutex);
                if (serving_clerk != -1) {
                    pthread_mutex_lock(&total_wait_time_mutex);
                    total_wait_time += time(NULL) - queue0->begin_wait_time;
                    pthread_mutex_unlock(&total_wait_time_mutex);
                    queue0 = removeCus(queue0);
                    printAll();
                    break;
                }
            }
        }
    }
    else if (shortest == 1) {
        queue1 = addCus(queue1, cus_info->cus_id, cus_info->sleep_time, cus_info->serv_time, time(NULL));
        q_size[1]++;
        printf("ARRIVAL: Added customer %d to queue 1\n", cus_info->cus_id);
        printAll();
        while(1) {
            pthread_cond_wait(&q1_cond, &queues_mutex);
            if (cus_info->cus_id == queue1->cus_id) {
                pthread_mutex_lock(&clerks_mutex);
                serving_clerk = clerkAvail(1);
                pthread_mutex_unlock(&clerks_mutex);
                if (serving_clerk != -1) {
                    pthread_mutex_lock(&total_wait_time_mutex);
                    total_wait_time += time(NULL) - queue1->begin_wait_time;
                    pthread_mutex_unlock(&total_wait_time_mutex);
                    queue1 = removeCus(queue1);
                    printAll();
                    break;
                }
            }
        } 
    }
    else if (shortest == 2) {
        queue2 = addCus(queue2, cus_info->cus_id, cus_info->sleep_time, cus_info->serv_time, time(NULL));
        q_size[2]++;
        printf("ARRIVAL: Added customer %d to queue 2\n", cus_info->cus_id);
        printAll();
        while(1) {
            pthread_cond_wait(&q2_cond, &queues_mutex);
            if (cus_info->cus_id == queue2->cus_id) {
                pthread_mutex_lock(&clerks_mutex);
                serving_clerk = clerkAvail(2);
                pthread_mutex_unlock(&clerks_mutex);
                if (serving_clerk != -1) {
                    pthread_mutex_lock(&total_wait_time_mutex);
                    total_wait_time += time(NULL) - queue2->begin_wait_time;
                    pthread_mutex_unlock(&total_wait_time_mutex);
                    queue2 = removeCus(queue2);
                    printAll();
                    break;
                }
            }
        }         
    }
    else if (shortest == 3) {
        queue3 = addCus(queue3, cus_info->cus_id, cus_info->sleep_time, cus_info->serv_time, time(NULL));
        q_size[3]++;
        printf("ARRIVAL: Added customer %d to queue 3\n", cus_info->cus_id);
        printAll();
        while(1) {
            pthread_cond_wait(&q3_cond, &queues_mutex);
            if (cus_info->cus_id == queue3->cus_id) {
                pthread_mutex_lock(&clerks_mutex);
                serving_clerk = clerkAvail(3);
                pthread_mutex_unlock(&clerks_mutex);
                if (serving_clerk != -1) {     
                    pthread_mutex_lock(&total_wait_time_mutex);
                    total_wait_time += time(NULL) - queue3->begin_wait_time;
                    pthread_mutex_unlock(&total_wait_time_mutex);        
                    queue3 = removeCus(queue3);
                    printAll();
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&queues_mutex);   
    pthread_mutex_unlock(&clerks_mutex);
    
    printf("CUSTOMER %d: Being served by clerk %d\n", cus_info->cus_id, serving_clerk);
    fflush(stdout);

    usleep(cus_info->serv_time * 1000000);

    printf("CUSTOMER %d: Leaving clerk %d\n", cus_info->cus_id, serving_clerk);
    fflush(stdout);

    pthread_mutex_lock(&queues_mutex);   
    num_cus--;
    printf("WORLD: Number of customers in the terminal %d\n", num_cus);
    fflush(stdout);
    pthread_mutex_unlock(&queues_mutex);

    if(serving_clerk == 0) pthread_cond_signal(&c0_cond);
    else if (serving_clerk == 1) pthread_cond_signal(&c1_cond);

    pthread_exit(NULL);
}

void *clerk_thread(void *id) {
    while(1) {
        int clerk_id = *((int *) id);
        int cus_id;
        int longest = -1;

        pthread_mutex_lock(&queues_mutex);

        while(longest == -1) {
            longest = pickQueue(0);
            pthread_mutex_unlock(&queues_mutex);
            usleep(1000000);
            pthread_mutex_lock(&queues_mutex);
        }
        
        pthread_mutex_lock(&clerks_mutex);  
        clerks[clerk_id] = longest;
        pthread_mutex_unlock(&clerks_mutex);


        if (longest == 0)  {
            cus_id = queue0->cus_id;
            q_size[0]--;
            printAll();
            pthread_mutex_unlock(&queues_mutex);
            pthread_cond_broadcast(&q0_cond);
        }
        else if (longest == 1) {
            cus_id = queue1->cus_id; 
            q_size[1]--;
            printAll();    
            pthread_mutex_unlock(&queues_mutex);
            pthread_cond_broadcast(&q1_cond);
        }
        else if (longest == 2) {
            cus_id = queue2->cus_id;  
            q_size[2]--;
            printAll();      
            pthread_mutex_unlock(&queues_mutex);
            pthread_cond_broadcast(&q2_cond);
        }
        else if (longest == 3) {
            cus_id = queue3->cus_id;
            q_size[3]--;
            printAll();        
            pthread_mutex_unlock(&queues_mutex);
            pthread_cond_broadcast(&q3_cond);
        }

        printf("CLERK %d: Serving customer %d from queue %d\n", clerk_id, cus_id, longest);
        fflush(stdout);
        longest = -1;

        if (clerk_id == 0) pthread_cond_wait(&c0_cond, &clerks_mutex);
        else if (clerk_id == 1) pthread_cond_wait(&c1_cond, &clerks_mutex);

        clerks[clerk_id] = -1;

        printf("CLERK %d: Done serving customer %d\n", clerk_id, cus_id);;
        
        pthread_mutex_unlock(&clerks_mutex);

        if(num_cus <= 0) pthread_exit(NULL);        
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    char *line = NULL;
    char *line_tok;
    size_t len = 0;

    if (argc != 2) {
        printf("ERROR: Incorrect number of arguments\nUSAGE:\n\tACS <filename>\n");
        exit(1);
    }

    FILE *file;
    file = fopen(argv[1], "r");
    if (file) {
        int line_count = 0;
        while((getline(&line, &len, file)) != -1) {
            if (line_count == 0) {
                num_cus = strtol(line, NULL, 10);
                ++line_count;
                continue;
            }

            ++line_count;
            int token_count = 0;
            char *token = strtok(line, ":,");
            int cus_id = 0;
            double sleep_time = 0;
            double serv_time = 0;
            while (token_count <= 2) {
                if (token_count == 0) cus_id = strtol(token, NULL, 10);
                else if (token_count == 1) sleep_time = strtol(token, NULL, 10)/(double)10;
                else if (token_count == 2) serv_time = strtol(token, NULL, 10)/(double)10;
                ++token_count;
                token = strtok(NULL, ":,");
            }
            if (cus_id < 0) {
                printf("WARNING: Invalid customer ID");
                exit(1);
            }
            if (sleep_time < 0) {
                printf("WARNING: Invalid sleep time\n");
                exit(1);
            }
            if (serv_time < 0) {
                printf("WARNING: Invalid service time\n");
                exit(1);
            }
            customers = addCus(customers, cus_id, sleep_time, serv_time, 0);
        }

        struct cus *cus_info = customers;
        pthread_t cus_threads[num_cus];
        pthread_t clerk_threads[num_clerks];
        int i;
        for (i = 0; i < num_queues; i++) q_size[i] = 0;

        for (i = 0; i < num_cus; i++) {
            printf("CREATION: Creating Customer %d\n", cus_info->cus_id);            
            if (pthread_create(&cus_threads[i], NULL, cus_thread, cus_info) != 0) {
                printf("ERROR: Unable to create customer thread\n");
                exit(1);
            }
            cus_info = cus_info->next;
        }

        for(i = 0; i < num_clerks; i++) {
            clerks[i] = -1;
            int *clerk_id = malloc(sizeof(*clerk_id));
            *clerk_id = i;
            printf("CREATION: Creating Clerk %d\n", *clerk_id);
            if (pthread_create(&clerk_threads[i], NULL, clerk_thread, clerk_id) != 0) {
                printf("ERROR: Unable to create clerk thread\n");
                exit(1);
            }
        }
    }
    while (1) {
        usleep(1000000);
        if(num_cus <= 0) {
            printf("Total Waiting Time %d\n", (int)total_wait_time/(int)num_cus);
            exit(0);
        }
    }
    exit(0);
}


/*********************
    Linked List Guidence from
        https://www.tutorialspoint.com/data_structures_algorithms/linked_list_program_in_c.htm
*********************/