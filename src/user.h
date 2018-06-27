#ifndef _USER_H
#define _USER_H 1

typedef struct _user {
    short added;
    int sockfd;
    char *name;
    struct _user *_next;
} user;

// mutex locks
static pthread_mutex_t user_lock_hashtable, user_lock_rename;

// add a new user
user* user_create(int sockfd);

// set user name
void user_set_name(user *u, char *name);

// remove user
void user_destroy(user *u);

// return next user if there is any
user* user_get_next(user *u);

// get user by socket id
user* user_get_by_sockfd(int sockfd);

// return the number of users in the hash table
unsigned long user_get_count();

#endif // _USER_H
