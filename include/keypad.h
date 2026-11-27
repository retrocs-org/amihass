#ifndef AH_KEYPAD_H
#define AH_KEYPAD_H
/* A small keypad window for alarm codes. The digits show as asterisks; the
 * code is returned in code (at most 16 characters) and the caller erases it
 * after use. Returns 1 for OK, 0 for Cancel or close. */
int ah_keypad(const char *title,char *code,unsigned int cap);
#endif
