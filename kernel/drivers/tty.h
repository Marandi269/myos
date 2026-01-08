/*
 * tty.h - TTY (Terminal) subsystem
 *
 * Implements terminal line discipline, input buffering, and
 * canonical/raw mode processing.
 */

#ifndef _TTY_H
#define _TTY_H

#include "types.h"

/* TTY buffer sizes */
#define TTY_INPUT_BUF_SIZE   256
#define TTY_OUTPUT_BUF_SIZE  4096
#define TTY_MAX_TTYS         8

/* Control characters */
#define CTRL(x)     ((x) & 0x1F)
#define TTY_EOF     CTRL('D')   /* ^D - End of file */
#define TTY_ERASE   0x7F        /* DEL - Erase character */
#define TTY_ERASE2  CTRL('H')   /* ^H - Backspace */
#define TTY_KILL    CTRL('U')   /* ^U - Kill line */
#define TTY_WERASE  CTRL('W')   /* ^W - Word erase */
#define TTY_INTR    CTRL('C')   /* ^C - Interrupt */
#define TTY_QUIT    CTRL('\\')  /* ^\ - Quit */
#define TTY_SUSP    CTRL('Z')   /* ^Z - Suspend */
#define TTY_STOP    CTRL('S')   /* ^S - Stop output */
#define TTY_START   CTRL('Q')   /* ^Q - Start output */

/* termios c_iflag bits */
#define IGNBRK      0x0001  /* Ignore BREAK condition */
#define BRKINT      0x0002  /* Signal interrupt on BREAK */
#define IGNPAR      0x0004  /* Ignore parity errors */
#define PARMRK      0x0008  /* Mark parity errors */
#define INPCK       0x0010  /* Input parity check */
#define ISTRIP      0x0020  /* Strip 8th bit off characters */
#define INLCR       0x0040  /* Map NL to CR on input */
#define IGNCR       0x0080  /* Ignore CR on input */
#define ICRNL       0x0100  /* Map CR to NL on input */
#define IXON        0x0400  /* Enable XON/XOFF flow control */
#define IXOFF       0x1000  /* Enable start/stop input control */
#define IUTF8       0x4000  /* Input is UTF-8 */

/* termios c_oflag bits */
#define OPOST       0x0001  /* Perform output processing */
#define ONLCR       0x0004  /* Map NL to CR-NL on output */
#define OCRNL       0x0008  /* Map CR to NL on output */
#define ONOCR       0x0010  /* No CR output at column 0 */
#define ONLRET      0x0020  /* NL performs CR function */

/* termios c_cflag bits */
#define CSIZE       0x0030  /* Character size mask */
#define CS5         0x0000  /* 5 bits */
#define CS6         0x0010  /* 6 bits */
#define CS7         0x0020  /* 7 bits */
#define CS8         0x0030  /* 8 bits */
#define CSTOPB      0x0040  /* Set two stop bits */
#define CREAD       0x0080  /* Enable receiver */
#define PARENB      0x0100  /* Parity enable */
#define PARODD      0x0200  /* Odd parity */
#define HUPCL       0x0400  /* Hang up on last close */
#define CLOCAL      0x0800  /* Ignore modem lines */

/* termios c_lflag bits */
#define ISIG        0x0001  /* Enable signals */
#define ICANON      0x0002  /* Canonical mode */
#define ECHO        0x0008  /* Enable echo */
#define ECHOE       0x0010  /* Echo erase as BS-SP-BS */
#define ECHOK       0x0020  /* Echo NL after kill character */
#define ECHONL      0x0040  /* Echo NL */
#define NOFLSH      0x0080  /* Disable flush after interrupt */
#define TOSTOP      0x0100  /* Stop background jobs on write */
#define ECHOCTL     0x0200  /* Echo control chars as ^X */
#define ECHOKE      0x0800  /* Echo kill by erasing line */
#define IEXTEN      0x8000  /* Enable extended input processing */

/* c_cc array indices */
#define VINTR       0   /* Interrupt character */
#define VQUIT       1   /* Quit character */
#define VERASE      2   /* Erase character */
#define VKILL       3   /* Kill character */
#define VEOF        4   /* End-of-file character */
#define VTIME       5   /* Timeout in deciseconds */
#define VMIN        6   /* Minimum number of characters */
#define VSWTC       7   /* Switch character */
#define VSTART      8   /* Start character */
#define VSTOP       9   /* Stop character */
#define VSUSP       10  /* Suspend character */
#define VEOL        11  /* End-of-line character */
#define VREPRINT    12  /* Reprint character */
#define VDISCARD    13  /* Discard character */
#define VWERASE     14  /* Word erase character */
#define VLNEXT      15  /* Literal next character */
#define VEOL2       16  /* Second end-of-line character */
#define NCCS        17  /* Size of c_cc array */

/* tcsetattr() optional_actions */
#define TCSANOW     0   /* Make changes immediately */
#define TCSADRAIN   1   /* Wait for output to drain */
#define TCSAFLUSH   2   /* Wait for output to drain, flush input */

/* tcflush() queue_selector */
#define TCIFLUSH    0   /* Flush input queue */
#define TCOFLUSH    1   /* Flush output queue */
#define TCIOFLUSH   2   /* Flush both queues */

/* tcflow() action */
#define TCOOFF      0   /* Suspend output */
#define TCOON       1   /* Resume output */
#define TCIOFF      2   /* Transmit STOP character */
#define TCION       3   /* Transmit START character */

/* Speed type */
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

/* TTY state */
typedef struct tty {
    int index;                              /* TTY index */
    int open_count;                         /* Open reference count */

    /* Line discipline buffers */
    char input_buf[TTY_INPUT_BUF_SIZE];     /* Input (keyboard) buffer */
    int input_head;                         /* Input buffer head */
    int input_tail;                         /* Input buffer tail */
    int input_count;                        /* Characters in input buffer */

    char canon_buf[TTY_INPUT_BUF_SIZE];     /* Canonical mode buffer */
    int canon_len;                          /* Canonical buffer length */

    char output_buf[TTY_OUTPUT_BUF_SIZE];   /* Output buffer */
    int output_head;
    int output_tail;
    int output_count;

    /* Terminal attributes */
    struct termios termios;

    /* Line state */
    int column;                             /* Current column */
    int stopped;                            /* Output stopped (^S) */
    int eof_seen;                           /* EOF character received */

    /* PTY link (if this is a PTY slave) */
    struct tty *pty_master;
    struct tty *pty_slave;
    int is_pty;

    /* Waiting processes */
    void *read_wait;
    void *write_wait;
} tty_t;

/* Initialize TTY subsystem */
void tty_init(void);

/* Get/create TTY by index */
tty_t *tty_get(int index);

/* Release TTY */
void tty_put(tty_t *tty);

/* Read from TTY */
ssize_t tty_read(tty_t *tty, char *buf, size_t count);

/* Write to TTY */
ssize_t tty_write(tty_t *tty, const char *buf, size_t count);

/* Process input character (from keyboard) */
void tty_input_char(tty_t *tty, char c);

/* Get/set terminal attributes */
int tty_tcgetattr(tty_t *tty, struct termios *termios_p);
int tty_tcsetattr(tty_t *tty, int optional_actions,
                   const struct termios *termios_p);

/* Flush terminal queues */
int tty_tcflush(tty_t *tty, int queue_selector);

/* Flow control */
int tty_tcflow(tty_t *tty, int action);

/* ioctl operations */
int tty_ioctl(tty_t *tty, unsigned long request, void *arg);

/* Default terminal settings */
void tty_set_defaults(struct termios *termios);

/* Get console TTY */
tty_t *tty_console(void);

#endif /* _TTY_H */
