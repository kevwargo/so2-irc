#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>

pthread_mutex_t TerminalLock;

/* Ta funkcja potrafi bezpiecznie wypisywać tekst na konsol
   (nie przerywając tekstu, który z klawiatury wprowadza użytkownik,
   ale z umową, że tekst ten czytamy funkcją readline) */
void safePrintf(char *fmt, ...)
{
    int need_hack = (rl_readline_state & RL_STATE_READCMD) > 0;
    char *saved_line;
    int saved_point;
    if (need_hack)
    {
        pthread_mutex_lock(&TerminalLock);
        saved_point = rl_point;
        saved_line = rl_copy_text(0, rl_end);
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    if (need_hack)
    {
        rl_restore_prompt();
        rl_replace_line(saved_line, 0);
        rl_point = saved_point;
        rl_redisplay();
        free(saved_line);
        pthread_mutex_unlock(&TerminalLock);
    }
}

int safeNewline(int count, int key)
{
    pthread_mutex_lock(&TerminalLock);
    int res = rl_newline(count, key);
    pthread_mutex_unlock(&TerminalLock);
    return res;
}

void readlineStartupHook()
{
    rl_bind_key(10, safeNewline);
    rl_bind_key(13, safeNewline);
}

/* Odpalamy na początku programu, żeby dalej readline i wątki ładnie sobie śmigały :) */
void initializePrompt()
{
    pthread_mutex_init(&TerminalLock, NULL);
    rl_startup_hook = (rl_hook_func_t *)readlineStartupHook;
}
