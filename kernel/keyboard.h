/*
 * keyboard.h - PS/2 Keyboard driver interface
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

/* Initialize keyboard */
void keyboard_init(void);

/* Keyboard interrupt handler */
void keyboard_handler(void);

#endif /* _KEYBOARD_H */
