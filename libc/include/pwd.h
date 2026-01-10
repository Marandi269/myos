/*
 * pwd.h - Password file access
 */

#ifndef _PWD_H
#define _PWD_H

#include <stddef.h>
#include <sys/stat.h>

/* Password structure */
struct passwd {
    char   *pw_name;        /* Username */
    char   *pw_passwd;      /* Password (usually 'x') */
    uid_t   pw_uid;         /* User ID */
    gid_t   pw_gid;         /* Group ID */
    char   *pw_gecos;       /* Real name */
    char   *pw_dir;         /* Home directory */
    char   *pw_shell;       /* Shell program */
};

/* Get password entry by UID */
struct passwd *getpwuid(uid_t uid);
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen,
               struct passwd **result);

/* Get password entry by name */
struct passwd *getpwnam(const char *name);
int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen,
               struct passwd **result);

/* Sequential access to password file */
void setpwent(void);
void endpwent(void);
struct passwd *getpwent(void);

#endif /* _PWD_H */
