/*
 * grp.c - Group file functions
 *
 * Simplified implementation with static groups.
 */

#include <grp.h>
#include <string.h>
#include <errno.h>
#include <syscall.h>

/* Static group entries */
static char root_grname[] = "root";
static char *root_members[] = { "root", NULL };

static struct group root_gr = {
    .gr_name = root_grname,
    .gr_passwd = "",
    .gr_gid = 0,
    .gr_mem = root_members,
};

static char nogroup_name[] = "nogroup";
static char *nogroup_members[] = { NULL };

static struct group nogroup_gr = {
    .gr_name = nogroup_name,
    .gr_passwd = "",
    .gr_gid = 65534,
    .gr_mem = nogroup_members,
};

struct group *getgrgid(gid_t gid) {
    if (gid == 0) {
        return &root_gr;
    }
    if (gid == 65534) {
        return &nogroup_gr;
    }
    return NULL;
}

int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen,
               struct group **result) {
    struct group *g = getgrgid(gid);
    if (!g) {
        *result = NULL;
        return 0;
    }

    size_t needed = strlen(g->gr_name) + 1 + strlen(g->gr_passwd) + 1;
    if (buflen < needed) {
        *result = NULL;
        return ERANGE;
    }

    char *ptr = buf;
    grp->gr_name = ptr;
    strcpy(ptr, g->gr_name);
    ptr += strlen(g->gr_name) + 1;

    grp->gr_passwd = ptr;
    strcpy(ptr, g->gr_passwd);

    grp->gr_gid = g->gr_gid;
    grp->gr_mem = g->gr_mem;  /* Point to static array */

    *result = grp;
    return 0;
}

struct group *getgrnam(const char *name) {
    if (!name) return NULL;
    if (strcmp(name, "root") == 0) {
        return &root_gr;
    }
    if (strcmp(name, "nogroup") == 0) {
        return &nogroup_gr;
    }
    return NULL;
}

int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen,
               struct group **result) {
    struct group *g = getgrnam(name);
    if (!g) {
        *result = NULL;
        return 0;
    }
    return getgrgid_r(g->gr_gid, grp, buf, buflen, result);
}

static int grent_index = 0;

void setgrent(void) {
    grent_index = 0;
}

void endgrent(void) {
    grent_index = 0;
}

struct group *getgrent(void) {
    switch (grent_index++) {
        case 0: return &root_gr;
        case 1: return &nogroup_gr;
        default: return NULL;
    }
}

int initgroups(const char *user, gid_t group) {
    (void)user;
    gid_t groups[1] = { group };
    return setgroups(1, groups);
}

int getgroups(int size, gid_t list[]) {
    if (size == 0) {
        return 1;  /* Return number of groups */
    }
    if (size < 1) {
        errno = EINVAL;
        return -1;
    }
    list[0] = 0;  /* Just root group */
    return 1;
}

int setgroups(size_t size, const gid_t *list) {
    (void)size;
    (void)list;
    /* Not fully implemented - just succeed */
    return 0;
}
