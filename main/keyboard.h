#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_scan(void);
void keyboard_task(void *pvParameters);

#endif
