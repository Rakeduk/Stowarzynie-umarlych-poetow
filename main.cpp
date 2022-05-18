#include <stdio.h>
#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <algorithm> 
#include <vector>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <queue>
#include <stdlib.h>
#include <bits/stdc++.h>
 
 
using namespace std;
 
// Tags
#define REQ 100 
#define ACT 110 
#define RELEASE 120
#define TAKEN 130 
 
// Constants
#define SIZE_OF_CIRCLE 3
#define NUMBER_OF_CIRCLES 4
 
 
// Structures
typedef struct req_in_queue{
    int lamport_value; 
    int proc_num; 
    int circle_num; 
}req_que;
 
typedef struct msg_in_vector{
    int last_message;
    int circle_num;
    int lamport_value;
}elem_vct;
 
vector<msg_in_vector> message_vector;
vector<req_in_queue> request_queue; 
int room[NUMBER_OF_CIRCLES]; //SIZE_OF_CIRCLE? 
 
// Variables
int current_size;
int current_rank;
int lamport = 0; 
int is_request_send = 0;
int circle = 0;
 
// Mutexs
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lamport_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vector_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t circle_mutex = PTHREAD_MUTEX_INITIALIZER;
 
 
// Functions
void *receive_loop(void  * arg);
void increment_lamport();
int generate_random_circle();
bool check_req_queue(int circle);
int get_lamport();
void clockUpdate(int valueFromMsg);
bool my_compare(req_que str_A,  req_que str_B);
void print_queue();
void update_message_vector(int proc_num, int circle, int message, int lamport);
void init_message_vector();
void print_vector();
bool check_msg_vector(bool is_first, int lamport);
bool taken_count(int circle);
void remove_req(int proc);
void check_if_present(int proc_num);
int increment_circle(int circle);
void decrement_room(int num_of_circle);
bool has_all_enter_room(int circle);
void increment_room(int num_of_circle);
void clear_room(int current_circle);
int get_my_number_of_circle();
void delete_my_taken(int proc_num, int circle, int message, int lamport);
void set_circle(int my_circle);
int get_circle();
void negative_circle();
void init_room_table();
 
int main(int argc, char **argv){
 
    // Checking if multi-thread is available
    int provided=0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    
    MPI_Comm_size(MPI_COMM_WORLD, &current_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &current_rank);
 
    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_loop, 0);
    
    
    // msg[0]-lamport msg[1]-circle_num  
    int msg[2];
    int delay;
    srand(current_rank);
    negative_circle();
    init_room_table();
 
 
    while(true){
        delay = rand() % 10000;
        usleep(delay);
        
        msg[1] = rand() % NUMBER_OF_CIRCLES;
        set_circle(msg[1]);
        increment_lamport();
        msg[0] = get_lamport();
        printf("%d, %d [main] == Szukam kola! Jestem trzezwy!\n", get_lamport(), current_rank);
        for(int i = 0; i < current_size; i++){  
            MPI_Send(&msg, 2, MPI_INT, i, REQ, MPI_COMM_WORLD);     
            //printf("%d, %d [main] == Sending REQ  to %d \n", get_lamport(), current_rank, i);
            
        }
        
        while(check_msg_vector(check_req_queue(get_circle()), msg[0]) == false || taken_count(get_circle()) == false){
            //printf("%d, %d [main] == Waiting for critical section \n", get_lamport(), current_rank );
            
            if(taken_count(get_circle()) == false && check_msg_vector(check_req_queue(get_circle()), msg[0]) == true){
                increment_lamport();
                //clear_room(msg[1]);
                msg[0] = get_lamport();
                msg[1] = increment_circle(get_circle());
                set_circle(msg[1]);
                for(int i = 0; i < current_size; i++){
                    MPI_Send(&msg, 2, MPI_INT, i, REQ, MPI_COMM_WORLD);
                    //if(current_rank == 0){
                    //printf("%d, %d [main] == Resending REQ with Lamport %d and circle %d \n", get_lamport(), current_rank, msg[0], get_circle());
                    //}
                }
                
            }
            usleep(5000);
        }
 
 
 
        increment_lamport();
        for(int i = 0; i < current_size; i++){
            msg[0] = get_lamport();
            MPI_Send(&msg, 2, MPI_INT, i, TAKEN, MPI_COMM_WORLD);
        }
        while(has_all_enter_room(get_circle()) == false){
            //pthread_mutex_lock(&room_mutex);
            //printf("%d, %d [main] == Waiting for all in room %d: %d/%d \n", get_lamport(), current_rank, get_circle() ,room[get_circle()], SIZE_OF_CIRCLE);
            //pthread_mutex_unlock(&room_mutex);
            usleep(5000);
        }
        //sleep(1);
        printf("%d, %d [main] ============ Kolo pelne! Idziemy do pokoju tworzyc poezje! Kolo --> %d \n", get_lamport(), current_rank, msg[1]);
        
        //clear_room(get_circle());
        negative_circle();
        increment_lamport();
        for(int i = 0; i < current_size; i++){
            msg[0] = get_lamport();
            MPI_Send(&msg, 2, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
            //if(current_rank == 0){
            //printf("%d, %d [main] == Sending RELEASE with Lamport %d and circle %d \n", get_lamport(), current_rank, msg[0], msg[1]);
            //}
        }
        //if(current_rank == 0){
            usleep(5000);
            printf("%d, %d [main]  == Pokoj pusty! Czas na kaca\n", get_lamport(), current_rank);
        //}
    } 
 
    MPI_Finalize();
    return 0;
}
 
 
void *receive_loop(void  * arg){
    int circle_i_want;
    int msg[2];
    init_message_vector();
    while(true){
        MPI_Status status;
        MPI_Recv(msg, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        update_message_vector(status.MPI_SOURCE, msg[1], status.MPI_TAG, msg[0]);
        clockUpdate(msg[0]);
    
        switch(status.MPI_TAG){
            case REQ:   
                req_que temp_queue;
                temp_queue.lamport_value = msg[0];
                temp_queue.proc_num = status.MPI_SOURCE;
                temp_queue.circle_num = msg[1];         
                
                check_if_present(status.MPI_SOURCE);
                pthread_mutex_lock(&queue_mutex); 
                request_queue.push_back(temp_queue);    
                sort(request_queue.begin(), request_queue.end(), my_compare);       
                pthread_mutex_unlock(&queue_mutex);
                //if(current_rank == 0){
                //printf("%d, %d [comm] == dodalem do kolejki REQ od %d / kolo: %d / lamport: %d \n", get_lamport(), current_rank, status.MPI_SOURCE, msg[1], msg[0]);
                //}
                increment_lamport();
                msg[0] = get_lamport();
    
                MPI_Send(&msg, 2, MPI_INT, status.MPI_SOURCE, ACT, MPI_COMM_WORLD);
 
                break;
                
            case ACT:
                //printf("%d, %d [comm]== Accept from %d, circle %d, lamport %d \n", get_lamport(), current_rank, status.MPI_SOURCE, msg[1], msg[0]);
                break;
            
            case RELEASE:
 
                //printf("%d, %d [comm]== RELASE from %d, circle %d, lamport %d \n", get_lamport(), current_rank, status.MPI_SOURCE, msg[1], msg[0]);
    
                delete_my_taken(status.MPI_SOURCE, msg[1], status.MPI_TAG, msg[0]);
                usleep(15000);
                decrement_room(msg[1]);
                break;
 
            case TAKEN:
                increment_room(msg[1]);
                //printf("%d, %d [comm] Taken from: %d Circle i get %d \n", get_lamport(), current_rank, status.MPI_SOURCE, msg[1]);
 
                remove_req(status.MPI_SOURCE);
 
                break;
            default:
                break;
        }
    }
 
    return 0;
}
 
void set_circle(int my_circle){
    pthread_mutex_lock(&circle_mutex);
    circle = my_circle;
    pthread_mutex_unlock(&circle_mutex);
}
 
 
int get_circle(){
    pthread_mutex_lock(&circle_mutex);
    int my_circle = circle;
    pthread_mutex_unlock(&circle_mutex);
    return my_circle;
}
 
void negative_circle(){
    pthread_mutex_lock(&circle_mutex);
    circle = -1; 
    pthread_mutex_unlock(&circle_mutex);
}
 
void delete_my_taken(int proc_num, int circle, int message, int lamport){
    pthread_mutex_lock(&vector_mutex);
    if(message_vector[proc_num].lamport_value < lamport){
        message_vector[proc_num].last_message = message;
        message_vector[proc_num].circle_num = circle; 
        message_vector[proc_num].lamport_value = lamport;
    }
    pthread_mutex_unlock(&vector_mutex);
}
 
 
int get_my_number_of_circle(){
    int my_circle;
    pthread_mutex_lock(&vector_mutex);
    my_circle = message_vector[current_rank].circle_num;
    pthread_mutex_unlock(&vector_mutex);
    return my_circle;
}
 
void increment_room(int num_of_circle){
    pthread_mutex_lock(&room_mutex);
    room[num_of_circle] += 1;
    pthread_mutex_unlock(&room_mutex);
}
 
void decrement_room(int num_of_circle){
    pthread_mutex_lock(&room_mutex);
    room[num_of_circle] -= 1;
    pthread_mutex_unlock(&room_mutex);
}
 
bool has_all_enter_room(int circle){
    bool output = false;
    pthread_mutex_lock(&room_mutex);
    if(room[circle] == SIZE_OF_CIRCLE){ /////////////////ZMIEN NA ==== !!!!!!!!!
        output = true;
    }
    else if(room[circle] > SIZE_OF_CIRCLE){
        //print_vector();
    }
    else{
        output = false;
    }
    pthread_mutex_unlock(&room_mutex);
 
    return output;
}
 
void clear_room(int current_circle){
    pthread_mutex_lock(&room_mutex);
    room[current_circle] = 0;
    for(int i = 0; i < SIZE_OF_CIRCLE; i++){
        if(i != current_circle){
            room[i] = room[i] % SIZE_OF_CIRCLE;
        }
    }
    pthread_mutex_unlock(&room_mutex);
}
 
void increment_lamport(){
    pthread_mutex_lock(&lamport_mutex);
    lamport += 1;
    pthread_mutex_unlock(&lamport_mutex);
}
 
int generate_random_circle(){
    int random_number = 0 + (rand() % NUMBER_OF_CIRCLES);
    return random_number;
}
 
int get_lamport(){
    pthread_mutex_lock(&lamport_mutex);
    int myLamport = lamport;
    pthread_mutex_unlock(&lamport_mutex);
    return myLamport;
}
 
bool check_req_queue(int circle){
    int first_proc_rank = 0;
    pthread_mutex_lock(&queue_mutex);
    if(!request_queue.empty()){
        for(long unsigned int i = 0; i < request_queue.size(); i++){
            if(request_queue[i].circle_num == circle){
                first_proc_rank = request_queue[i].proc_num;
                //printf("%d, %d Process z pierwszym req dla kolka %d: %d \n", get_lamport(), current_rank, circle, first_proc_rank);
                break;
            }
        }
    }   
    //else{
    //  printf("%d, %d REQUEST QUE EMPTY \n", get_lamport(), current_rank);
    //}
    pthread_mutex_unlock(&queue_mutex);
    if(first_proc_rank == current_rank){
        return true;
    }
    //if(current_rank == 0){
        
        //printf("%d, %d == ERROR QUE \n", get_lamport(), current_rank);
        //print_queue();
        
        return false;
}
 
bool check_msg_vector(bool is_first, int lamport){   // do poprawy
    if(is_first == true){
        bool all_older = true;
        pthread_mutex_lock(&vector_mutex);
        for(long unsigned int i = 0; i < message_vector.size(); i++){
            if(message_vector[i].lamport_value < lamport){
                all_older = false;
                //if(current_rank == 0){
                //  
                //printf("%d, %d == [ERROR VCT] \n", get_lamport(), current_rank);
                //}
                break;
            }
        }
        pthread_mutex_unlock(&vector_mutex);
        return all_older;
    }
    return false;
}
 
bool taken_count(int circle){
    int sum_of_taken = 0;
    int sum_of_relase = 0; 
    pthread_mutex_lock(&vector_mutex);
    for(long unsigned int i = 0; i < message_vector.size(); i++){
            if(message_vector[i].circle_num == circle && message_vector[i].last_message == RELEASE){
                sum_of_relase = sum_of_relase + 1;
            }
        }
    if(sum_of_relase == 0 || sum_of_relase == SIZE_OF_CIRCLE){
        for(long unsigned int i = 0; i < message_vector.size(); i++){
            if(message_vector[i].circle_num == circle && message_vector[i].last_message == TAKEN){
                sum_of_taken = sum_of_taken + 1;
            }
        }   
    }
    else{
        sum_of_taken = SIZE_OF_CIRCLE;
    }   
    pthread_mutex_unlock(&vector_mutex);
    if(sum_of_taken < SIZE_OF_CIRCLE){
        return true;
    }
    return false; 
}
 
void clockUpdate(int valueFromMsg) 
{
    pthread_mutex_lock(&lamport_mutex);
    lamport = max(lamport, valueFromMsg) + 1;
    pthread_mutex_unlock(&lamport_mutex);
}
 
 
bool my_compare(req_que str_A,  req_que str_B){
        if (str_A.lamport_value < str_B.lamport_value)
            return str_A.lamport_value < str_B.lamport_value;
 
        if (str_A.lamport_value == str_B.lamport_value)
            return str_A.proc_num < str_B.proc_num;
        else
            return str_A.lamport_value < str_B.lamport_value;
}
 
void print_queue(){
    pthread_mutex_lock(&queue_mutex);
    for(long unsigned int i = 0; i < request_queue.size(); i++){
        printf("%d,  %d, L: %d, P: %d, K: %d   !!", get_lamport(), current_rank, request_queue[i].lamport_value, request_queue[i].proc_num, request_queue[i].circle_num);
    }
    printf("\n");
    pthread_mutex_unlock(&queue_mutex);
}
 
void print_vector(){
    pthread_mutex_lock(&vector_mutex);
    for(long unsigned int i = 0; i < message_vector.size(); i++){
        printf("%d, %d, Last M: %d, Circle: %d, Lamport: %d!!",get_lamport(), current_rank, message_vector[i].last_message, message_vector[i].circle_num, message_vector[i].lamport_value);
    }
    printf("\n");
    pthread_mutex_unlock(&vector_mutex);
}
 
void update_message_vector(int proc_num, int circle, int message, int lamport){
    pthread_mutex_lock(&vector_mutex);
    if(message_vector[proc_num].last_message != TAKEN && message_vector[proc_num].lamport_value < lamport){
        message_vector[proc_num].last_message = message;
        message_vector[proc_num].circle_num = circle; 
        message_vector[proc_num].lamport_value = lamport;
    }
    else if(message_vector[proc_num].last_message == TAKEN && message == ACT){
        if(message_vector[proc_num].lamport_value < lamport){
            message_vector[proc_num].lamport_value = lamport;
        }
    }
    pthread_mutex_unlock(&vector_mutex);
}
 
void init_message_vector(){
    pthread_mutex_lock(&vector_mutex);
    msg_in_vector temp_msg; 
    temp_msg.last_message = 0;
    temp_msg.circle_num = 0;
    temp_msg.lamport_value = 0;
    for(int i = 0; i < current_size; i++){
        message_vector.push_back(temp_msg); 
    }
    pthread_mutex_unlock(&vector_mutex);
}
 
void remove_req(int proc){
    pthread_mutex_lock(&queue_mutex);
    for(long unsigned int i = 0; i < request_queue.size(); i++){
        if(request_queue[i].proc_num == proc){
            request_queue.erase(request_queue.begin() + i);
        }
    }
    
    pthread_mutex_unlock(&queue_mutex);
}
 
void check_if_present(int proc_num){
    pthread_mutex_lock(&queue_mutex);
    for(long unsigned int i = 0; i < request_queue.size(); i++){
        if(request_queue[i].proc_num == proc_num){
            request_queue.erase(request_queue.begin() + i);
        }
    }
    pthread_mutex_unlock(&queue_mutex);
}
 
int increment_circle(int circle){
    if(circle == NUMBER_OF_CIRCLES - 1){
        return 0;
    }
    else{
        return circle + 1;
    }
}
 
void init_room_table(){
    pthread_mutex_lock(&room_mutex);
    for(int i = 0; i < NUMBER_OF_CIRCLES; i++){ //SIZE_OF_CIRCLE
        room[i] = 0;
    }
    pthread_mutex_unlock(&room_mutex);
}
