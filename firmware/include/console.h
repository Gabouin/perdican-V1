#ifndef PERDICAN_CONSOLE_H
#define PERDICAN_CONSOLE_H

void console_init(void);
void console_banner(void);

/* Non-blocking: consumes whatever the host has sent and runs any complete
 * line. Call it from the main loop. */
void console_poll(void);

#endif
