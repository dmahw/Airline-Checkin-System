#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
                                                //Changing defines WILL NOT work for scaling up or down performance
#define NUM_CLERKS 2                            //Number of clerks
#define NUM_QUEUES 4                            //Number of queues

int terminal_size;                              //Number of customers expected to arrive
int num_cus;                                    //Number of customers expected to arrive - left
double total_wait_time = 0;                     //Total waiting time 
struct cus *customers = NULL;                   //General customer database
int clerks[NUM_CLERKS];                         //Clerk id
struct cus *queue[NUM_QUEUES];                  //Queue database
int q_size[NUM_QUEUES];                         //Size of each queue
pthread_mutex_t queues_mutex;                   //Mutex lock for queues
pthread_mutex_t clerks_mutex;                   //Mutex lock for clerks
pthread_mutex_t total_wait_time_mutex;          //Mutex lock for total wait time
pthread_cond_t q0_cond;                         //Convar for queue0
pthread_cond_t q1_cond;                         //Convar for queue1
pthread_cond_t q2_cond;                         //Convar for queue2
pthread_cond_t q3_cond;                         //Convar for queue3
pthread_cond_t c0_cond;                         //Convar for clerk1
pthread_cond_t c1_cond;                         //Convar for clerk2

struct cus {                                    //Struct to hold customer info
    int cus_id;                                 //Customer ID
    double sleep_time;                          //Arrival time
    double serv_time;                           //Service time
    double begin_wait_time;                     //Time started waiting
    struct cus *next;                           //Next customer in queue or database
};

void printList(struct cus *list) {              //print the linked list
    struct cus *ptr = list;
    while(ptr != NULL) {
        printf("%d: %f %f\n", ptr->cus_id, ptr->sleep_time, ptr->serv_time);
        ptr = ptr->next;
    }
}

//return actual size of queue
int size(struct cus *list) {
    struct cus *ptr = list;
    int size = 0;
    while(ptr != NULL) {
        ++size;
        ptr = ptr->next;
    }
    return size;
}

/*********************
    Linked List Guidence from
        https://www.tutorialspoint.com/data_structures_algorithms/linked_list_program_in_c.htm
*********************/

//add customer to queue or database
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

//remove customer from the front of the queue
struct cus *removeCus(struct cus *list) {
    if(list == NULL) return NULL;
    list = list->next; 
    return list;
}

/********************************
    END OF LINKED LIST GUIDENCE
********************************/

//pick the shortest or longest queue
int pickQueue(int shortest) {
    int pick = 0;

    //If no one in a queue, return -1 as longest pick
    if(shortest == 0) {
        if((q_size[0] == 0) && (q_size[1] == 0) && (q_size[2] == 0) && (q_size[3] == 0)){
            return -1;
        }
    }
    
    //If all queues are the same size, return a random queue
    if((q_size[0] == q_size[1]) && (q_size[1] == q_size[2]) && (q_size[2] == q_size[3])){
        pick = rand() % 4;
        return pick;
    }


    int i;
    if (shortest == 1) {    //Determine smallest queue, if tie, random pick
        for(i = 0; i < 4; i++) {
            if (q_size[pick] > q_size[i]) pick = i;
            else if (q_size[pick] == q_size[i]) {
                if (rand() % 2 == 1) pick = i;
            }
        }
    } else {               //Determine longest queue, if tie, random pick 
        for(i = 0; i < 4; i++) {
            if (q_size[pick] < q_size[i]) pick = i;
            else if (q_size[pick] == q_size[i]) {
                if (rand() % 2 == 1) pick = i;
            }
        }
    }
    return pick;
}

//determine which clerk is available and has selected which queue
int clerkAvail(int q_num) {
    int i;
    for (i = 0; i < NUM_CLERKS; i++) {
        if (clerks[i] == q_num) {
            clerks[i] = -1;
            return i;
        }
    }
    return -1;
}

//general print statement for troubleshooting
void printAll() {
    int i = 0;
    for (i = 0; i < NUM_QUEUES; i++) {
        printf("LOG: Queue %d\n", i);
        printList(queue[i]);
    }
    fflush(stdout);
    for (i = 0; i < NUM_QUEUES; i++) {
        printf("LOG: Queue %d size: %d\n", i, q_size[i]);
    }
    fflush(stdout);
}

//customer thread
void *cus_thread(void *cus) {
    struct cus *cus_info = cus;                                 //storage for customer info
    int serving_clerk;                                          //store which clerk is serving me
    usleep(cus_info->sleep_time * 1000000);                     //sleep for arrival time
    printf("A customer arrives: customer ID %2d.\n", cus_info->cus_id);
    fflush(stdout);

    pthread_mutex_lock(&queues_mutex);                          //lock the queues for editing
    int shortest = pickQueue(1);                                //determine which is shortest queue
    queue[shortest] = addCus(queue[shortest], cus_info->cus_id, cus_info->sleep_time, cus_info->serv_time, time(NULL));
    q_size[shortest]++;                                         //increment queue size
    printf("A customer enters a queue: the queue ID %1d, and the length of the queue%2d.\n", shortest, q_size[shortest]);
    fflush(stdout);

    //while in the queue
    while(1) {                                   
        if (shortest == 0) pthread_cond_wait(&q0_cond, &queues_mutex);              //wait for specific queue convar
        if (shortest == 1) pthread_cond_wait(&q1_cond, &queues_mutex);              //
        if (shortest == 2) pthread_cond_wait(&q2_cond, &queues_mutex);              //
        if (shortest == 3) pthread_cond_wait(&q3_cond, &queues_mutex);              //
        if (cus_info->cus_id == queue[shortest]->cus_id) {                          //determine whether you are being served
            pthread_mutex_lock(&clerks_mutex);                                      //lock the clerks
            serving_clerk = clerkAvail(shortest);                                   //determine which clerk is serving
            pthread_mutex_unlock(&clerks_mutex);                                    //unlock the clerks
            if (serving_clerk != -1) {                                              //determine which clerk is serving
                pthread_mutex_lock(&total_wait_time_mutex);                         //lock total wait time
                total_wait_time += time(NULL) - queue[shortest]->begin_wait_time;   //append wait to total wait time
                pthread_mutex_unlock(&total_wait_time_mutex);                       //unlock total wait time
                queue[shortest] = removeCus(queue[shortest]);                       //remove self from queue
                break;                                                              //exit queue
            }
        }
    }

    pthread_mutex_unlock(&queues_mutex);                //unlock queues
    pthread_mutex_unlock(&clerks_mutex);                //unlock clerks

    usleep(cus_info->serv_time * 1000000);              //sleep for service time

    pthread_mutex_lock(&queues_mutex);                  //lock queues
    num_cus--;                                          //leave the terminal checkin
    pthread_mutex_unlock(&queues_mutex);                //unlock queues

    if(serving_clerk == 0) pthread_cond_signal(&c0_cond);           //signal serving clerk that you have finished
    else if (serving_clerk == 1) pthread_cond_signal(&c1_cond);

    pthread_exit(NULL);                                             //customer exits
}

//clerk thread
void *clerk_thread(void *id) {
    while(1) {
        int clerk_id = *((int *) id);                   //clerk id
        int cus_id;                                     //storage for customer id
        int longest = -1;

        pthread_mutex_lock(&queues_mutex);              //lock queues
        while(longest == -1) {                          //determine longest queue, if no one check later
            longest = pickQueue(0);                     //pick longest queue
            if(longest != -1) break;                    //if no queues are empty, continue with serving
            pthread_mutex_unlock(&queues_mutex);        //unlock queues for others to join or check
            if(num_cus <= 0) pthread_exit(NULL);        //if all expected customers have arrived and left. job is done
            usleep(100000);                             //sleep to check queues later
            pthread_mutex_lock(&queues_mutex);          //lock queues for checking queues
        }

        pthread_mutex_lock(&clerks_mutex);              //lock clerks
        clerks[clerk_id] = longest;                     //set your serving clerk and serving queue
        pthread_mutex_unlock(&clerks_mutex);            //unlock clerks

        cus_id = queue[longest]->cus_id;                //determine which customer to serve
        q_size[longest]--;                              //update queue size
        pthread_mutex_unlock(&queues_mutex);            //unlock queues
        if (longest == 0) pthread_cond_broadcast(&q0_cond);     //broadcast to all customer in specific longest queue
        if (longest == 1) pthread_cond_broadcast(&q1_cond);     //customers determine whether it is first in queue
        if (longest == 2) pthread_cond_broadcast(&q2_cond);     //
        if (longest == 3) pthread_cond_broadcast(&q3_cond);     //

        printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d.\n", (double)time(NULL), cus_id, clerk_id);
        fflush(stdout);
        longest = -1;                                           //reset longest pick

        if (clerk_id == 0) pthread_cond_wait(&c0_cond, &clerks_mutex);      //wait on customer to finish serving itself
        else if (clerk_id == 1) pthread_cond_wait(&c1_cond, &clerks_mutex); //convar depending which clerk you are

        clerks[clerk_id] = -1;                          //Set your serving clerk status to busy

        printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d.\n", (double)time(NULL), cus_id, clerk_id);
        fflush(stdout);

        pthread_mutex_unlock(&clerks_mutex);            //unlock clerks

        if(num_cus <= 0) pthread_exit(NULL);            //exit once all expected customers have left
    }
    pthread_exit(NULL);
}

//main
int main(int argc, char *argv[]) {
    srand(time(NULL));                      //random number
    char *line = NULL;                      //line storage
    size_t len = 0;

    if (argc != 2) {                        //number of arguments required to run
        printf("ERROR: Incorrect number of arguments\nUSAGE:\n\tACS <filename>\n");
        exit(1);
    }

    FILE *file;
    file = fopen(argv[1], "r");
    if (file) {                             //open specified file for data
        int line_count = 0;
        while((getline(&line, &len, file)) != -1) {         //for each line
            if (line_count == 0) {                          //if first line, store value as expected customers
                terminal_size = strtol(line, NULL, 10);
                ++line_count;
                continue;
            }

            ++line_count;
            int token_count = 0;                            //token settings
            char *token = strtok(line, ":,");               //
            int cus_id = 0;                                 //storage for customer id
            double sleep_time = 0;                          //storage for sleep time
            double serv_time = 0;                           //storage for service time
            while (token_count <= 2) {                      //parsing each line for customer id, arrival time, and service time
                if (token_count == 0) cus_id = strtol(token, NULL, 10);
                else if (token_count == 1) sleep_time = strtol(token, NULL, 10)/(double)10;
                else if (token_count == 2) serv_time = strtol(token, NULL, 10)/(double)10;
                ++token_count;
                token = strtok(NULL, ":,");
            }
            if (cus_id < 0) {                               //if found negative parameters for specified customer, ignore customer
                printf("WARNING: Invalid customer ID. Ignoring customer.\n");
                terminal_size--;
                continue;
            }
            if (sleep_time < 0) {                           //if found negative parameters for specified customer, ignore customer
                printf("WARNING: Invalid sleep time. Ignoring customer.\n");
                terminal_size--;
                continue;
            }
            if (serv_time < 0) {                            //if found negative parameters for specified customer, ignore customer
                printf("WARNING: Invalid service time. Ignoring customer\n");
                terminal_size--;
                continue;
            }                                               //add each customer into a general customer database
            customers = addCus(customers, cus_id, sleep_time, serv_time, 0);
        }
                                        //initialize all quueues, clerks, mutexes, convars
        int init_mutex_error = 0;       //if mutex error
        int init_cond_error = 0;        //if convar error
        if (pthread_mutex_init(&queues_mutex, NULL) != 0) init_mutex_error = 1;
        if (pthread_mutex_init(&clerks_mutex, NULL) != 0) init_mutex_error = 1;
        if (pthread_mutex_init(&total_wait_time_mutex, NULL) != 0) init_mutex_error = 1;
        if (pthread_cond_init(&q0_cond, NULL) != 0) init_cond_error = 1;
        if (pthread_cond_init(&q1_cond, NULL) != 0) init_cond_error = 1;
        if (pthread_cond_init(&q2_cond, NULL) != 0) init_cond_error = 1;
        if (pthread_cond_init(&q3_cond, NULL) != 0) init_cond_error = 1;
        if (pthread_cond_init(&c0_cond, NULL) != 0) init_cond_error = 1;
        if (pthread_cond_init(&c1_cond, NULL) != 0) init_cond_error = 1;
        if (init_mutex_error == 1) {    //exit if mutex error
            printf("ERROR: Unable to initialize mutexes\n");
            exit(1);
        }
        if (init_cond_error == 1) {     //exit if convar error
            printf("ERROR: Unable to initialize convars\n");
            exit(1);
        }

        num_cus = terminal_size;                //set number of customers
        struct cus *cus_info = customers;       //point to a customer in general database
        pthread_t cus_threads[terminal_size];   //customer threads
        pthread_t clerk_threads[NUM_CLERKS];    //clerk thread

        int i;
        for (i = 0; i < NUM_QUEUES; i++) q_size[i] = 0;     //for each queue, set size to 0
        
        for (i = 0; i < terminal_size; i++) {               //for each customer, create a thread
            if (pthread_create(&cus_threads[i], NULL, cus_thread, cus_info) != 0) { //each thread will have own customer info
                printf("ERROR: Unable to create customer thread\n");
                exit(1);                                    //exit if customer thread fails to be created
            }
            cus_info = cus_info->next;                      //next customer
        }

        for(i = 0; i < NUM_CLERKS; i++) {                           //For each clerk
            clerks[i] = -1;                                         //Set clerk busy to true
            int *clerk_id = malloc(sizeof(*clerk_id));              //storage for clerk id
            *clerk_id = i;                                          //clerk id
            if (pthread_create(&clerk_threads[i], NULL, clerk_thread, clerk_id) != 0) { //create a clerk thread
                printf("ERROR: Unable to create clerk thread\n");
                exit(1);                                            //exit if clerk thread fails to be created
            }
        }

        while (1) {
            usleep(100000);                 //every 100ms, check if all customers and clerks left
            int i;                          //join with every customer and clerk thread once they finish
            for(i = 0; i < terminal_size; i++) pthread_join(cus_threads[i], NULL);
            for(i = 0; i < NUM_CLERKS; i++) pthread_join(clerk_threads[i], NULL);
            
            
            printf("The average waiting time for all customers in the system is: %.2f\n", (double)total_wait_time/(double)terminal_size);
            
            pthread_mutex_unlock(&queues_mutex);
            pthread_mutex_unlock(&clerks_mutex);
            pthread_mutex_unlock(&total_wait_time_mutex);

            int dest_cond_error = 0;        //destroy all mutexes and convars
            if (pthread_cond_destroy(&q0_cond) != 0) dest_cond_error = 1;
            if (pthread_cond_destroy(&q1_cond) != 0) dest_cond_error = 1;
            if (pthread_cond_destroy(&q2_cond) != 0) dest_cond_error = 1;
            if (pthread_cond_destroy(&q3_cond) != 0) dest_cond_error = 1;
            if (pthread_cond_destroy(&c0_cond) != 0) dest_cond_error = 1;
            if (pthread_cond_destroy(&c1_cond) != 0) dest_cond_error = 1;
            if (dest_cond_error == 1) {     //Exit if failed to destroy convars
                printf("ERROR: Unable to destroy convars\n");
                exit(1);
            }
            if (fclose(file) != 0) {
                printf("ERROR: Unable to close file");
                exit(1);
            }
            exit(0);                        //If all goes well, exit.
        }
    }
    exit(0);
}