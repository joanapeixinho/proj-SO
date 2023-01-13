#include "producer-consumer/producer-consumer.h"
//TODO implement the rest of the programs in .h files
int pcq_create(pc_queue_t *queue, size_t capacity){
    if (capacity == 0) {
        return -1;
    }
    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;
    
    //TODO: change (void *) to struct of registry
    if(queue->pcq_buffer = malloc(capacity * sizeof(void*)) == NULL){
        free(queue->pcq_buffer);
        return -1;
    }

    if(pthread_mutex_init(&queue->pcq_current_size_lock, NULL) != 0){
        free(queue->pcq_buffer);
        return -1;
    }
    if(pthread_mutex_init(&queue->pcq_head_lock, NULL) != 0){
        free(queue->pcq_buffer);
        pthread_mutex_destroy(&queue->pcq_current_size_lock);
        return -1;
    }
    if(pthread_mutex_init(&queue->pcq_tail_lock, NULL) != 0){
        free(queue->pcq_buffer);
        pthread_mutex_destroy(&queue->pcq_current_size_lock);
        pthread_mutex_destroy(&queue->pcq_head_lock);
        return -1;
    }
    if(pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL) != 0){
        free(queue->pcq_buffer);
        pthread_mutex_destroy(&queue->pcq_current_size_lock);
        pthread_mutex_destroy(&queue->pcq_head_lock);
        pthread_mutex_destroy(&queue->pcq_tail_lock);
        return -1;
    }
    if(pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL) != 0){
        free(queue->pcq_buffer);
        pthread_mutex_destroy(&queue->pcq_current_size_lock);
        pthread_mutex_destroy(&queue->pcq_head_lock);
        pthread_mutex_destroy(&queue->pcq_tail_lock);
        pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
        return -1;
    }
    if(pthread_cond_init(&queue->pcq_pusher_condvar, NULL) != 0){
        free(queue->pcq_buffer);
        pthread_mutex_destroy(&queue->pcq_current_size_lock);
        pthread_mutex_destroy(&queue->pcq_head_lock);
        pthread_mutex_destroy(&queue->pcq_tail_lock);
        pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
        pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
        return -1;
    }
    if(pthread_cond_init(&queue->pcq_popper_condvar, NULL) != 0){
        free(queue->pcq_buffer);
        pthread_mutex_destroy(&queue->pcq_current_size_lock);
        pthread_mutex_destroy(&queue->pcq_head_lock);
        pthread_mutex_destroy(&queue->pcq_tail_lock);
        pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
        pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
        pthread_cond_destroy(&queue->pcq_pusher_condvar);
        return -1;
    }

    return 0;
}


int pcq_destroy(pc_queue_t *queue){
    free(queue->pcq_buffer);
    pthread_mutex_destroy(&queue->pcq_current_size_lock);
    pthread_mutex_destroy(&queue->pcq_head_lock);
    pthread_mutex_destroy(&queue->pcq_tail_lock);
    pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
    pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
    pthread_cond_destroy(&queue->pcq_pusher_condvar);
    pthread_cond_destroy(&queue->pcq_popper_condvar);
    return 0;
}

int pcq_enqueue(pc_queue_t *queue, void *elem){
    pthread_mutex_lock(&queue->pcq_pusher_condvar_lock);
    //Wait until there is space in the queue
    while(queue->pcq_current_size == queue->pcq_capacity){
        pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock);
    }
    pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock);

    pthread_mutex_lock(&queue->pcq_tail_lock);
    //Save element at the tail of the queue and increment tail
    queue->pcq_buffer[queue->pcq_tail] = elem;
    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    pthread_mutex_unlock(&queue->pcq_tail_lock);

    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size++;
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    pthread_mutex_lock(&queue->pcq_popper_condvar_lock);
    pthread_cond_signal(&queue->pcq_popper_condvar);
    pthread_mutex_unlock(&queue->pcq_popper_condvar_lock);

    return 0;
}

void *pcq_dequeue(pc_queue_t *queue){
    pthread_mutex_lock(&queue->pcq_popper_condvar_lock);
    //Wait until there is an element in the queue
    while(queue->pcq_current_size == 0){
        pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_popper_condvar_lock);
    }
    pthread_mutex_unlock(&queue->pcq_popper_condvar_lock);

    pthread_mutex_lock(&queue->pcq_head_lock);
    //Pop element at the head of the queue and increment head
    void *elem = queue->pcq_buffer[queue->pcq_head];
    queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;
    pthread_mutex_unlock(&queue->pcq_head_lock);

    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size--;
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    pthread_mutex_lock(&queue->pcq_pusher_condvar_lock);
    pthread_cond_signal(&queue->pcq_pusher_condvar);
    pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock);

    return elem;
}