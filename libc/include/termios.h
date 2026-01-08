/*
 * termios.h - Terminal I/O interface
 */

#ifndef _TERMIOS_H
#define _TERMIOS_H

/* Control characters */
#define VINTR       0
#define VQUIT       1
#define VERASE      2
#define VKILL       3
#define VEOF        4
#define VTIME       5
#define VMIN        6
#define VSWTC       7
#define VSTART      8
#define VSTOP       9
#define VSUSP       10
#define VEOL        11
#define VREPRINT    12
#define VDISCARD    13
#define VWERASE     14
#define VLNEXT      15
#define VEOL2       16
#define NCCS        17

/* c_iflag bits */
#define IGNBRK      0x0001
#define BRKINT      0x0002
#define IGNPAR      0x0004
#define PARMRK      0x0008
#define INPCK       0x0010
#define ISTRIP      0x0020
#define INLCR       0x0040
#define IGNCR       0x0080
#define ICRNL       0x0100
#define IUCLC       0x0200
#define IXON        0x0400
#define IXANY       0x0800
#define IXOFF       0x1000
#define IMAXBEL     0x2000
#define IUTF8       0x4000

/* c_oflag bits */
#define OPOST       0x0001
#define OLCUC       0x0002
#define ONLCR       0x0004
#define OCRNL       0x0008
#define ONOCR       0x0010
#define ONLRET      0x0020
#define OFILL       0x0040
#define OFDEL       0x0080
#define NLDLY       0x0100
#define NL0         0x0000
#define NL1         0x0100
#define CRDLY       0x0600
#define CR0         0x0000
#define CR1         0x0200
#define CR2         0x0400
#define CR3         0x0600
#define TABDLY      0x1800
#define TAB0        0x0000
#define TAB1        0x0800
#define TAB2        0x1000
#define TAB3        0x1800
#define XTABS       0x1800
#define BSDLY       0x2000
#define BS0         0x0000
#define BS1         0x2000
#define VTDLY       0x4000
#define VT0         0x0000
#define VT1         0x4000
#define FFDLY       0x8000
#define FF0         0x0000
#define FF1         0x8000

/* c_cflag bits */
#define CBAUD       0x100F
#define B0          0x0000
#define B50         0x0001
#define B75         0x0002
#define B110        0x0003
#define B134        0x0004
#define B150        0x0005
#define B200        0x0006
#define B300        0x0007
#define B600        0x0008
#define B1200       0x0009
#define B1800       0x000A
#define B2400       0x000B
#define B4800       0x000C
#define B9600       0x000D
#define B19200      0x000E
#define B38400      0x000F

#define CSIZE       0x0030
#define CS5         0x0000
#define CS6         0x0010
#define CS7         0x0020
#define CS8         0x0030
#define CSTOPB      0x0040
#define CREAD       0x0080
#define PARENB      0x0100
#define PARODD      0x0200
#define HUPCL       0x0400
#define CLOCAL      0x0800

/* c_lflag bits */
#define ISIG        0x0001
#define ICANON      0x0002
#define XCASE       0x0004
#define ECHO        0x0008
#define ECHOE       0x0010
#define ECHOK       0x0020
#define ECHONL      0x0040
#define NOFLSH      0x0080
#define TOSTOP      0x0100
#define ECHOCTL     0x0200
#define ECHOPRT     0x0400
#define ECHOKE      0x0800
#define FLUSHO      0x1000
#define PENDIN      0x4000
#define IEXTEN      0x8000

/* tcsetattr optional_actions */
#define TCSANOW     0
#define TCSADRAIN   1
#define TCSAFLUSH   2

/* tcflush queue_selector */
#define TCIFLUSH    0
#define TCOFLUSH    1
#define TCIOFLUSH   2

/* tcflow action */
#define TCOOFF      0
#define TCOON       1
#define TCIOFF      2
#define TCION       3

/* Types */
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

/* termios structure */
struct termios {
    tcflag_t c_iflag;       /* Input modes */
    tcflag_t c_oflag;       /* Output modes */
    tcflag_t c_cflag;       /* Control modes */
    tcflag_t c_lflag;       /* Local modes */
    cc_t c_line;            /* Line discipline */
    cc_t c_cc[NCCS];        /* Control characters */
    speed_t c_ispeed;       /* Input speed */
    speed_t c_ospeed;       /* Output speed */
};

/* Window size structure */
struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/* Functions */
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);
int tcflow(int fd, int action);
int tcsendbreak(int fd, int duration);
int tcdrain(int fd);

speed_t cfgetispeed(const struct termios *termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);

void cfmakeraw(struct termios *termios_p);

/* Make stdin a raw terminal */
int isatty(int fd);

#endif /* _TERMIOS_H */
