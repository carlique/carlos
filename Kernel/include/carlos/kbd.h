#pragma once

void kbd_init(void);
int  kbd_try_getc(char *out); // returns 1 if ASCII char available, else 0