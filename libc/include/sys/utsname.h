/*
 * sys/utsname.h - System information structure
 */

#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#define _UTSNAME_LENGTH 65

struct utsname {
    char sysname[_UTSNAME_LENGTH];      /* Operating system name */
    char nodename[_UTSNAME_LENGTH];     /* Network node hostname */
    char release[_UTSNAME_LENGTH];      /* Operating system release */
    char version[_UTSNAME_LENGTH];      /* Operating system version */
    char machine[_UTSNAME_LENGTH];      /* Hardware identifier */
    char domainname[_UTSNAME_LENGTH];   /* NIS or YP domain name */
};

int uname(struct utsname *buf);

#endif /* _SYS_UTSNAME_H */
