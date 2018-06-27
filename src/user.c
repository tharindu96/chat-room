#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "user.h"

#define TABLE_SIZE 512
#define RELE_PRIME (TABLE_SIZE / 2) + 1

#define HASH(k) (k * RELE_PRIME) % TABLE_SIZE

static user* hash_table[TABLE_SIZE] = { 0 };
static unsigned long count = 0;

// add a new user
user* user_create(int sockfd);

void user_set_name(user *u, char *name);

// remove user
void user_destroy(user *u);

// return next user if there is any
user* user_get_next(user *u);

user* user_create(int sockfd)
{
    int h = HASH(sockfd);
    user *u = malloc(sizeof(user));
    u->added = 0;
    u->sockfd = sockfd;
    u->name = NULL;
    u->_next = NULL;
    pthread_mutex_lock(&user_lock_hashtable);
    user *hs = hash_table[h];
    if (hs == NULL) {
        hash_table[h] = u;
    } else {
        while (hs->_next == NULL) {
            hs = hs->_next;
        }
        hs->_next = u;
    }
    count += 1;
    pthread_mutex_unlock(&user_lock_hashtable);
    return u;
}

void user_set_name(user *u, char *name)
{
    if (u == NULL) return;
    pthread_mutex_lock(&user_lock_rename);
    if (u->name != NULL) free(u->name);
    u->name = strdup(name);
    pthread_mutex_unlock(&user_lock_rename);
}

void user_destroy(user *u)
{
    if (u == NULL) return;
    int h = HASH(u->sockfd);
    pthread_mutex_lock(&user_lock_hashtable);
    user *hs = hash_table[h];
    user *ps = hs;
    if (hs == NULL) return;
    while (hs != NULL && hs->sockfd != u->sockfd) {
        ps = hs;
        hs = hs->_next;
    }
    if (hs == NULL) return;
    if (ps == hs) {
        hash_table[h] = ps->_next;
    } else {
        ps->_next = hs->_next;
    }
    count -= 1;
    pthread_mutex_unlock(&user_lock_hashtable);
    if (u->name != NULL)
        free(u->name);
    free(u);
}

user* user_get_next(user *u)
{
    int h, j, i;
    if (u != NULL) {
        if (u->_next != NULL) return u->_next;
        h = HASH(u->sockfd);
        j = h;
        i = (h + 1) % TABLE_SIZE;
    } else {
        j = TABLE_SIZE - 1;
        i = 0;
    }
    while (1) {
        if (hash_table[i] != NULL) return hash_table[i];
        i = (i + 1) % TABLE_SIZE;
        if (i == (j + 1) % TABLE_SIZE) return NULL;
    }
}

user* user_get_by_sockfd(int sockfd)
{
    int h = HASH(sockfd);
    user *u = hash_table[h];
    while (u != NULL && u->sockfd != sockfd) {
        u = u->_next;
    }
    return u;
}

unsigned long user_get_count()
{
    return count;
}
