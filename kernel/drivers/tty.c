/*
 * tty.c - TTY (Terminal) subsystem implementation
 */

#include "drivers/tty.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "serial.h"

/* TTY array */
static tty_t ttys[TTY_MAX_TTYS];
static tty_t *console_tty = NULL;

/* Set default terminal settings */
void tty_set_defaults(struct termios *t) {
    memset(t, 0, sizeof(struct termios));

    /* Input modes: CR->NL, enable XON/XOFF */
    t->c_iflag = ICRNL | IXON;

    /* Output modes: NL->CRNL, post-process */
    t->c_oflag = OPOST | ONLCR;

    /* Control modes: 8 bits, enable receiver */
    t->c_cflag = CS8 | CREAD;

    /* Local modes: canonical, echo, signals */
    t->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE;

    /* Control characters */
    t->c_cc[VINTR]    = CTRL('C');
    t->c_cc[VQUIT]    = CTRL('\\');
    t->c_cc[VERASE]   = 0x7F;       /* DEL */
    t->c_cc[VKILL]    = CTRL('U');
    t->c_cc[VEOF]     = CTRL('D');
    t->c_cc[VTIME]    = 0;
    t->c_cc[VMIN]     = 1;
    t->c_cc[VSTART]   = CTRL('Q');
    t->c_cc[VSTOP]    = CTRL('S');
    t->c_cc[VSUSP]    = CTRL('Z');
    t->c_cc[VWERASE]  = CTRL('W');
    t->c_cc[VREPRINT] = CTRL('R');

    /* Default speeds */
    t->c_ispeed = 38400;
    t->c_ospeed = 38400;
}

/* Initialize TTY subsystem */
void tty_init(void) {
    memset(ttys, 0, sizeof(ttys));

    for (int i = 0; i < TTY_MAX_TTYS; i++) {
        ttys[i].index = i;
        tty_set_defaults(&ttys[i].termios);
    }

    /* Set up console TTY (tty0) */
    console_tty = &ttys[0];
    console_tty->open_count = 1;

    kprintf("[TTY] Initialized %d terminals\n", TTY_MAX_TTYS);
}

/* Get console TTY */
tty_t *tty_console(void) {
    return console_tty;
}

/* Get/create TTY by index */
tty_t *tty_get(int index) {
    if (index < 0 || index >= TTY_MAX_TTYS) {
        return NULL;
    }
    ttys[index].open_count++;
    return &ttys[index];
}

/* Release TTY */
void tty_put(tty_t *tty) {
    if (tty && tty->open_count > 0) {
        tty->open_count--;
    }
}

/* Output a character (to serial/display) */
static void tty_output_char(tty_t *tty, char c) {
    /* Post-process output */
    if (tty->termios.c_oflag & OPOST) {
        if (c == '\n' && (tty->termios.c_oflag & ONLCR)) {
            serial_putchar('\r');
            tty->column = 0;
        }
    }

    serial_putchar(c);

    if (c == '\n' || c == '\r') {
        tty->column = 0;
    } else if (c == '\t') {
        tty->column = (tty->column + 8) & ~7;
    } else if (c >= ' ') {
        tty->column++;
    } else if (c == '\b' && tty->column > 0) {
        tty->column--;
    }
}

/* Output a string */
static void tty_output_string(tty_t *tty, const char *s) {
    while (*s) {
        tty_output_char(tty, *s++);
    }
}

/* Echo a character with control char handling */
static void tty_echo_char(tty_t *tty, char c) {
    if (!(tty->termios.c_lflag & ECHO)) {
        return;
    }

    if (c == '\n') {
        tty_output_char(tty, '\n');
    } else if (c < ' ' && c != '\t' && (tty->termios.c_lflag & ECHOCTL)) {
        /* Echo control characters as ^X */
        tty_output_char(tty, '^');
        tty_output_char(tty, c + '@');
    } else {
        tty_output_char(tty, c);
    }
}

/* Erase a character from display */
static void tty_erase_char(tty_t *tty) {
    if (tty->termios.c_lflag & ECHOE) {
        tty_output_string(tty, "\b \b");
    }
}

/* Erase the entire line */
static void tty_erase_line(tty_t *tty, int len) {
    if (tty->termios.c_lflag & ECHOKE) {
        /* Erase by printing backspaces */
        for (int i = 0; i < len; i++) {
            char c = tty->canon_buf[i];
            if (c < ' ' && c != '\t' && (tty->termios.c_lflag & ECHOCTL)) {
                tty_output_string(tty, "\b \b\b \b");  /* Erase ^X */
            } else {
                tty_output_string(tty, "\b \b");
            }
        }
    } else if (tty->termios.c_lflag & ECHOK) {
        tty_output_char(tty, '\n');
    }
}

/* Check if character is a word delimiter */
static int is_word_delim(char c) {
    return c == ' ' || c == '\t';
}

/* Process input character */
void tty_input_char(tty_t *tty, char c) {
    /* XON/XOFF flow control */
    if (tty->termios.c_iflag & IXON) {
        if (c == tty->termios.c_cc[VSTOP]) {
            tty->stopped = 1;
            return;
        }
        if (c == tty->termios.c_cc[VSTART]) {
            tty->stopped = 0;
            return;
        }
    }

    /* Strip 8th bit */
    if (tty->termios.c_iflag & ISTRIP) {
        c &= 0x7F;
    }

    /* CR -> NL translation */
    if (c == '\r') {
        if (tty->termios.c_iflag & IGNCR) {
            return;  /* Ignore CR */
        }
        if (tty->termios.c_iflag & ICRNL) {
            c = '\n';
        }
    } else if (c == '\n' && (tty->termios.c_iflag & INLCR)) {
        c = '\r';
    }

    /* Signal handling */
    if (tty->termios.c_lflag & ISIG) {
        if (c == tty->termios.c_cc[VINTR]) {
            /* TODO: Send SIGINT */
            tty_echo_char(tty, c);
            tty_output_char(tty, '\n');
            tty->canon_len = 0;
            return;
        }
        if (c == tty->termios.c_cc[VQUIT]) {
            /* TODO: Send SIGQUIT */
            tty_echo_char(tty, c);
            tty_output_char(tty, '\n');
            tty->canon_len = 0;
            return;
        }
        if (c == tty->termios.c_cc[VSUSP]) {
            /* TODO: Send SIGTSTP */
            tty_echo_char(tty, c);
            tty_output_char(tty, '\n');
            return;
        }
    }

    /* Canonical mode processing */
    if (tty->termios.c_lflag & ICANON) {
        /* EOF */
        if (c == tty->termios.c_cc[VEOF]) {
            tty->eof_seen = 1;
            /* Make data available without newline */
            return;
        }

        /* Erase character */
        if (c == tty->termios.c_cc[VERASE] || c == TTY_ERASE2) {
            if (tty->canon_len > 0) {
                char erased = tty->canon_buf[--tty->canon_len];
                if (erased < ' ' && erased != '\t' &&
                    (tty->termios.c_lflag & ECHOCTL)) {
                    tty_erase_char(tty);  /* Erase ^X (two chars) */
                }
                tty_erase_char(tty);
            }
            return;
        }

        /* Kill line */
        if (c == tty->termios.c_cc[VKILL]) {
            tty_erase_line(tty, tty->canon_len);
            tty->canon_len = 0;
            return;
        }

        /* Word erase */
        if (c == tty->termios.c_cc[VWERASE]) {
            /* Skip trailing whitespace */
            while (tty->canon_len > 0 &&
                   is_word_delim(tty->canon_buf[tty->canon_len - 1])) {
                tty->canon_len--;
                tty_erase_char(tty);
            }
            /* Erase word */
            while (tty->canon_len > 0 &&
                   !is_word_delim(tty->canon_buf[tty->canon_len - 1])) {
                tty->canon_len--;
                tty_erase_char(tty);
            }
            return;
        }

        /* Buffer the character */
        if (tty->canon_len < TTY_INPUT_BUF_SIZE - 1) {
            tty->canon_buf[tty->canon_len++] = c;
            tty_echo_char(tty, c);
        }

        /* Line complete on newline */
        if (c == '\n') {
            /* Transfer to input buffer */
            for (int i = 0; i < tty->canon_len; i++) {
                if (tty->input_count < TTY_INPUT_BUF_SIZE) {
                    tty->input_buf[tty->input_tail] = tty->canon_buf[i];
                    tty->input_tail = (tty->input_tail + 1) % TTY_INPUT_BUF_SIZE;
                    tty->input_count++;
                }
            }
            tty->canon_len = 0;
            /* TODO: Wake up waiting readers */
        }
    } else {
        /* Raw mode - direct to input buffer */
        if (tty->input_count < TTY_INPUT_BUF_SIZE) {
            tty->input_buf[tty->input_tail] = c;
            tty->input_tail = (tty->input_tail + 1) % TTY_INPUT_BUF_SIZE;
            tty->input_count++;

            if (tty->termios.c_lflag & ECHO) {
                tty_output_char(tty, c);
            }
            /* TODO: Wake up waiting readers */
        }
    }
}

/* Read from TTY */
ssize_t tty_read(tty_t *tty, char *buf, size_t count) {
    if (!tty || !buf || count == 0) {
        return -1;
    }

    size_t read_count = 0;

    /* In canonical mode, wait for complete line */
    if (tty->termios.c_lflag & ICANON) {
        /* Wait for input (simplified - no blocking) */
        if (tty->input_count == 0 && !tty->eof_seen) {
            return 0;  /* Would block */
        }

        /* Handle EOF */
        if (tty->eof_seen && tty->input_count == 0) {
            tty->eof_seen = 0;
            return 0;  /* EOF */
        }

        /* Read up to newline */
        while (read_count < count && tty->input_count > 0) {
            char c = tty->input_buf[tty->input_head];
            tty->input_head = (tty->input_head + 1) % TTY_INPUT_BUF_SIZE;
            tty->input_count--;
            buf[read_count++] = c;
            if (c == '\n') {
                break;
            }
        }
    } else {
        /* Raw mode */
        int vmin = tty->termios.c_cc[VMIN];
        int vtime = tty->termios.c_cc[VTIME];

        /* Simplified: just return what's available */
        (void)vmin;
        (void)vtime;

        while (read_count < count && tty->input_count > 0) {
            buf[read_count++] = tty->input_buf[tty->input_head];
            tty->input_head = (tty->input_head + 1) % TTY_INPUT_BUF_SIZE;
            tty->input_count--;
        }
    }

    return read_count;
}

/* Write to TTY */
ssize_t tty_write(tty_t *tty, const char *buf, size_t count) {
    if (!tty || !buf) {
        return -1;
    }

    /* Check if output stopped */
    if (tty->stopped) {
        return 0;  /* Would block */
    }

    for (size_t i = 0; i < count; i++) {
        tty_output_char(tty, buf[i]);
    }

    return count;
}

/* Get terminal attributes */
int tty_tcgetattr(tty_t *tty, struct termios *termios_p) {
    if (!tty || !termios_p) {
        return -1;
    }
    memcpy(termios_p, &tty->termios, sizeof(struct termios));
    return 0;
}

/* Set terminal attributes */
int tty_tcsetattr(tty_t *tty, int optional_actions,
                   const struct termios *termios_p) {
    if (!tty || !termios_p) {
        return -1;
    }

    switch (optional_actions) {
    case TCSANOW:
        /* Immediate */
        break;
    case TCSADRAIN:
        /* Wait for output - simplified */
        break;
    case TCSAFLUSH:
        /* Wait for output, flush input */
        tty->input_head = tty->input_tail = 0;
        tty->input_count = 0;
        break;
    default:
        return -1;
    }

    memcpy(&tty->termios, termios_p, sizeof(struct termios));
    return 0;
}

/* Flush terminal queues */
int tty_tcflush(tty_t *tty, int queue_selector) {
    if (!tty) {
        return -1;
    }

    switch (queue_selector) {
    case TCIFLUSH:
        tty->input_head = tty->input_tail = 0;
        tty->input_count = 0;
        tty->canon_len = 0;
        break;
    case TCOFLUSH:
        tty->output_head = tty->output_tail = 0;
        tty->output_count = 0;
        break;
    case TCIOFLUSH:
        tty->input_head = tty->input_tail = 0;
        tty->input_count = 0;
        tty->canon_len = 0;
        tty->output_head = tty->output_tail = 0;
        tty->output_count = 0;
        break;
    default:
        return -1;
    }

    return 0;
}

/* Flow control */
int tty_tcflow(tty_t *tty, int action) {
    if (!tty) {
        return -1;
    }

    switch (action) {
    case TCOOFF:
        tty->stopped = 1;
        break;
    case TCOON:
        tty->stopped = 0;
        break;
    case TCIOFF:
        /* Send STOP character */
        if (tty->termios.c_iflag & IXOFF) {
            tty_output_char(tty, tty->termios.c_cc[VSTOP]);
        }
        break;
    case TCION:
        /* Send START character */
        if (tty->termios.c_iflag & IXOFF) {
            tty_output_char(tty, tty->termios.c_cc[VSTART]);
        }
        break;
    default:
        return -1;
    }

    return 0;
}

/* ioctl operations */
int tty_ioctl(tty_t *tty, unsigned long request, void *arg) {
    if (!tty) {
        return -1;
    }

    /* Common ioctl numbers (Linux compatible) */
    #define TCGETS      0x5401
    #define TCSETS      0x5402
    #define TCSETSW     0x5403
    #define TCSETSF     0x5404
    #define TIOCGWINSZ  0x5413
    #define TIOCSWINSZ  0x5414

    switch (request) {
    case TCGETS:
        return tty_tcgetattr(tty, (struct termios *)arg);

    case TCSETS:
        return tty_tcsetattr(tty, TCSANOW, (const struct termios *)arg);

    case TCSETSW:
        return tty_tcsetattr(tty, TCSADRAIN, (const struct termios *)arg);

    case TCSETSF:
        return tty_tcsetattr(tty, TCSAFLUSH, (const struct termios *)arg);

    case TIOCGWINSZ:
    case TIOCSWINSZ:
        /* Window size - return default 80x24 */
        if (arg) {
            struct winsize {
                unsigned short ws_row;
                unsigned short ws_col;
                unsigned short ws_xpixel;
                unsigned short ws_ypixel;
            } *ws = (struct winsize *)arg;
            if (request == TIOCGWINSZ) {
                ws->ws_row = 24;
                ws->ws_col = 80;
                ws->ws_xpixel = 0;
                ws->ws_ypixel = 0;
            }
            return 0;
        }
        break;

    default:
        return -1;  /* ENOTTY */
    }

    return -1;
}
