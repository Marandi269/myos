/*
 * pwd.c - Password file functions
 *
 * Simplified implementation with static root user.
 */

#include <pwd.h>
#include <string.h>
#include <errno.h>

/* Static password entry for root */
static char root_name[] = "root";
static char root_passwd[] = "x";
static char root_gecos[] = "root";
static char root_dir[] = "/root";
static char root_shell[] = "/bin/sh";

static struct passwd root_pw = {
    .pw_name = root_name,
    .pw_passwd = root_passwd,
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_gecos = root_gecos,
    .pw_dir = root_dir,
    .pw_shell = root_shell,
};

/* Nobody user */
static char nobody_name[] = "nobody";
static char nobody_passwd[] = "x";
static char nobody_gecos[] = "nobody";
static char nobody_dir[] = "/";
static char nobody_shell[] = "/bin/false";

static struct passwd nobody_pw = {
    .pw_name = nobody_name,
    .pw_passwd = nobody_passwd,
    .pw_uid = 65534,
    .pw_gid = 65534,
    .pw_gecos = nobody_gecos,
    .pw_dir = nobody_dir,
    .pw_shell = nobody_shell,
};

struct passwd *getpwuid(uid_t uid) {
    if (uid == 0) {
        return &root_pw;
    }
    if (uid == 65534) {
        return &nobody_pw;
    }
    return NULL;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen,
               struct passwd **result) {
    struct passwd *p = getpwuid(uid);
    if (!p) {
        *result = NULL;
        return 0;
    }

    /* Copy to provided buffer */
    size_t needed = strlen(p->pw_name) + 1 + strlen(p->pw_passwd) + 1 +
                    strlen(p->pw_gecos) + 1 + strlen(p->pw_dir) + 1 +
                    strlen(p->pw_shell) + 1;

    if (buflen < needed) {
        *result = NULL;
        return ERANGE;
    }

    char *ptr = buf;
    pwd->pw_name = ptr;
    strcpy(ptr, p->pw_name);
    ptr += strlen(p->pw_name) + 1;

    pwd->pw_passwd = ptr;
    strcpy(ptr, p->pw_passwd);
    ptr += strlen(p->pw_passwd) + 1;

    pwd->pw_uid = p->pw_uid;
    pwd->pw_gid = p->pw_gid;

    pwd->pw_gecos = ptr;
    strcpy(ptr, p->pw_gecos);
    ptr += strlen(p->pw_gecos) + 1;

    pwd->pw_dir = ptr;
    strcpy(ptr, p->pw_dir);
    ptr += strlen(p->pw_dir) + 1;

    pwd->pw_shell = ptr;
    strcpy(ptr, p->pw_shell);

    *result = pwd;
    return 0;
}

struct passwd *getpwnam(const char *name) {
    if (!name) return NULL;
    if (strcmp(name, "root") == 0) {
        return &root_pw;
    }
    if (strcmp(name, "nobody") == 0) {
        return &nobody_pw;
    }
    return NULL;
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen,
               struct passwd **result) {
    struct passwd *p = getpwnam(name);
    if (!p) {
        *result = NULL;
        return 0;
    }
    return getpwuid_r(p->pw_uid, pwd, buf, buflen, result);
}

static int pwent_index = 0;

void setpwent(void) {
    pwent_index = 0;
}

void endpwent(void) {
    pwent_index = 0;
}

struct passwd *getpwent(void) {
    switch (pwent_index++) {
        case 0: return &root_pw;
        case 1: return &nobody_pw;
        default: return NULL;
    }
}
