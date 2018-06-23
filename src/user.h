#ifndef _USER_H
#define _USER_H 1

typedef struct _user {
    int sockfd;
    char *name;
    struct _user *_next;
} user;

// add a new user
user* user_create(int sockfd, char *name);

// remove user
void user_destroy(user *u);

// return next user if there is any
user* user_get_next(user *u);

#endif // _USER_H
