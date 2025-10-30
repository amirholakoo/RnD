#ifndef KEYPAD_H
#define KEYPAD_H

#include "driver/gpio.h"

// keypad pins order:
// c2 r1 c1 r4 c3 r3 r2

#define KEYPAD_ROW_1 GPIO_NUM_16
#define KEYPAD_ROW_2 GPIO_NUM_4
#define KEYPAD_ROW_3 GPIO_NUM_5
#define KEYPAD_ROW_4 GPIO_NUM_7
#define KEYPAD_COL_1 GPIO_NUM_15
#define KEYPAD_COL_2 GPIO_NUM_17
#define KEYPAD_COL_3 GPIO_NUM_6



// Keypad layout
static const char keypad_layout[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

// Function prototypes
void keypad_init(void);
char keypad_get_key(void);

#endif // KEYPAD_H