/*
 * grp.h - Group file access
 */

#ifndef _GRP_H
#define _GRP_H

#include <stddef.h>
#include <sys/stat.h>

/* Group structure */
struct group {
    char   *gr_name;        /* Group name */
    char   *gr_passwd;      /* Password (usually empty or 'x') */
    gid_t   gr_gid;         /* Group ID */
    char  **gr_mem;         /* Group members */
};

/* Get group entry by GID */
struct group *getgrgid(gid_t gid);
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen,
               struct group **result);

/* Get group entry by name */
struct group *getgrnam(const char *name);
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen,
               struct group **result);

/* Sequential access to group file */
void setgrent(void);
void endgrent(void);
struct group *getgrent(void);

/* Get groups for user */
int initgroups(const char *user, gid_t group);
int getgroups(int size, gid_t list[]);
int setgroups(size_t size, const gid_t *list);

#endif /* _GRP_H */
