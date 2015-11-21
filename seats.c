#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "seats.h"
#include "semaphore.h"

#define STANDBY_SIZE 10

seat_t* seat_header = NULL;

int empty_seats = 0;

standby_t* standby_front;
standby_t* standby_rear;
int standby_size = 0;//FIX IT

m_sem_t sem;


char seat_state_to_char(seat_state_t);

void list_seats(char* buf, int bufsize)
{
    seat_t* curr = seat_header;
    int index = 0;
    while(curr != NULL && index < bufsize+ strlen("%d %c,"))
    {
        pthread_mutex_lock(&curr->lock);
        int length = snprintf(buf+index, bufsize-index, 
                "%d %c,", curr->id, seat_state_to_char(curr->state));
        pthread_mutex_unlock(&curr->lock);
        if (length > 0)
            index = index + length;
        curr = curr->next;
    }
    if (index > 0)
        snprintf(buf+index-1, bufsize-index-1, "\n");
    else
        snprintf(buf, bufsize, "No seats not found\n\n");
}

void print_standby_list() {

}

void view_seat(char* buf, int bufsize,  int seat_id, int customer_id, int customer_priority)
{
    seat_t* curr = seat_header;
    while(curr != NULL)
    {
        if(curr->id == seat_id)
        {   
            pthread_mutex_lock(&curr->lock);
            if(curr->state == AVAILABLE || (curr->state == PENDING && curr->customer_id == customer_id))
            {    
                //control the empty seats
                if (curr->state == AVAILABLE)
                    empty_seats--;

                snprintf(buf, bufsize, "Confirm seat: %d %c ?\n\n",
                        curr->id, seat_state_to_char(curr->state));
                curr->state = PENDING;
                curr->customer_id = customer_id;
                curr->start_time = clock();
            }
            else
            {
                sem_wait(&sem);
                //if there is no empty seat
                if (!empty_seats) {
                    //check standby list and if possible put it into list
                    if (standby_size && curr->state == PENDING) {
                        standby_t* standby_req = (standby_t*)malloc(sizeof(standby_t));
                        standby_req->customer_id = customer_id;
                        if (standby_front == NULL)
                            standby_front = standby_rear = standby_req;
                        else {
                            standby_rear->next = standby_req;
                            standby_rear = standby_req;
                        } 
                        standby_size--;
                    }
                    else {
                        snprintf(buf, bufsize, "Seat unavailable\n\n");
                    }
                }
                sem_post(&sem);
                snprintf(buf, bufsize, "Seat unavailable\n\n");
            }
            pthread_mutex_unlock(&curr->lock);
            return;
        }
        curr = curr->next;
    }
    snprintf(buf, bufsize, "Requested seat not found\n\n");
    return;
}

void confirm_seat(char* buf, int bufsize, int seat_id, int customer_id, int customer_priority)
{
    seat_t* curr = seat_header;
    while(curr != NULL)
    {
        if(curr->id == seat_id)
        {
            pthread_mutex_lock(&curr->lock);
            if(curr->state == PENDING && curr->customer_id == customer_id )
            {
                snprintf(buf, bufsize, "Seat confirmed: %d %c\n\n",
                        curr->id, seat_state_to_char(curr->state));
                curr->state = OCCUPIED;
            }
            else if(curr->customer_id != customer_id )
            {
                snprintf(buf, bufsize, "Permission denied - seat held by another user\n\n");
            }
            else if(curr->state != PENDING)
            {
                snprintf(buf, bufsize, "No pending request\n\n");
            }
            pthread_mutex_unlock(&curr->lock);
            return;
        }
        curr = curr->next;
    }
    snprintf(buf, bufsize, "Requested seat not found\n\n");
    
    return;
}

void cancel(char* buf, int bufsize, int seat_id, int customer_id, int customer_priority)
{
    seat_t* curr = seat_header;
    while(curr != NULL)
    {
        if(curr->id == seat_id)
        {
            pthread_mutex_lock(&curr->lock);
            //when you want to cancel, first check standby list
            if(curr->state == PENDING && curr->customer_id == customer_id )
            {
                snprintf(buf, bufsize, "Seat request cancelled: %d %c\n\n",
                        curr->id, seat_state_to_char(curr->state));
                sem_wait(&sem);

                standby_t* cur = standby_front;
                if (cur != NULL) {
                    curr->customer_id = cur->customer_id;

                    if (standby_front == standby_rear)
                        standby_front = standby_rear = NULL;
                    else 
                        standby_front = standby_front->next;
                    standby_size++;
                }
                else {
                    curr->state = AVAILABLE;
                    curr->customer_id = -1;
                    empty_seats++;
                }
                sem_post(&sem);
            }
            else if(curr->customer_id != customer_id )
            {
                snprintf(buf, bufsize, "Permission denied - seat held by another user\n\n");
            }
            else if(curr->state != PENDING)
            {
                snprintf(buf, bufsize, "No pending request\n\n");
            }
            pthread_mutex_unlock(&curr->lock);
            return;
        }
        curr = curr->next;
    }
    snprintf(buf, bufsize, "Seat not found\n\n");
    
    return;
}
//clean up
void check_seats() {
    clock_t cur_time = clock();//record current time
    seat_t* curr = seat_header;
    while (curr != NULL) {
        clock_t diff, elapsed;
        pthread_mutex_lock(&curr->lock);
        diff = cur_time - curr->start_time;
        elapsed = diff * 1000 / CLOCKS_PER_SEC;//calculate elapsed time
        //if current seat is pending and wait time is greater than 5 sec
        //check standby list or make the seat available
        if (curr->state == PENDING && elapsed >= 5) {
            curr->start_time = cur_time;//reset the start time
            sem_wait(&sem);
            standby_t* cur = standby_front;
            if (cur != NULL) {
                curr->customer_id = cur->customer_id;
                curr->start_time = cur_time;
                if (standby_front == standby_rear)
                    standby_front = standby_rear = NULL;
                else 
                    standby_front = standby_front->next;
                standby_size++;
            }
            else {
                curr->state = AVAILABLE;
                curr->customer_id = -1;
                empty_seats++;
            }
            sem_post(&sem);
        }
        pthread_mutex_unlock(&curr->lock);
        curr = curr->next;
    }
    return;
}

void load_seats(int number_of_seats)
{
    seat_t* curr = NULL;
    standby_size = STANDBY_SIZE;//flag to count the size of availabe size of standby list
    empty_seats = number_of_seats;//flag to count available seats
    sem_init(&sem, 1);//set to 1
    standby_front = NULL;
    standby_rear = NULL;
    int i;
    for(i = 0; i < number_of_seats; i++)
    {   
        seat_t* temp = (seat_t*) malloc(sizeof(seat_t));
        temp->id = i;
        temp->customer_id = -1;
        temp->state = AVAILABLE;
        temp->next = NULL;
        pthread_mutex_init(&temp->lock, NULL);
        
        if (seat_header == NULL)
        {
            seat_header = temp;
        }
        else
        {
            curr-> next = temp;
        }
        curr = temp;
    }
}

void unload_seats()
{
    seat_t* curr = seat_header;
    sem_destroy(&sem);
    while(curr != NULL)
    {
        seat_t* temp = curr;
        curr = curr->next;
        pthread_mutex_destroy(&temp->lock);
        free(temp);
    }
}

char seat_state_to_char(seat_state_t state)
{
    switch(state)
    {
        case AVAILABLE:
            return 'A';
        case PENDING:
            return 'P';
        case OCCUPIED:
            return 'O';
    }
    return '?';
}
