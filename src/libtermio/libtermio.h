/*                     L I B T E R M I O . H
 * BRL-CAD
 *
 * Published in 1986-2025 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @addtogroup libtermio
 *
 * @brief
 *
 * Minimal cross-platform terminal I/O - provides essential terminal handling
 * functions for interactive text editors/tools across Windows and POSIX
 * platforms.
 *
 * There are two "tiers" of API provided here - the simplest is the five
 * functions wrapping the basic operations needed by the majority of BRL-CAD
 * utilities that use termio-style operations.  They are very simplistic and
 * intended to work on as many platforms as possible.
 *
 * The second tier is more elaborate and powerful, capable of supporting
 * BRL-CAD's minimalist interactive text editor (used as a fallback if a
 * platform should happen not to have a viable console editor installed.)  It
 * has a much richer feature set, including the ability to support editor
 * interactions in various Windows consoles, but the logic is correspondingly
 * more complex.
 *
 * Public Optional Helpers:
 *    const char *termio_input_environment(void);
 *    void        termio_runtime_sanitize(void);
 *    void        termio_drain_startup_noise(void);
 *    void        termio_debug_init(void);
 *    void        termio_debug_shutdown(void);
 *
 * Usage:
 *    #define LIBTERMIO_IMPLEMENTATION
 *    #include "libtermio.h"
 *
 *    termio_save_tty(STDIN_FILENO);
 *    termio_set_raw(STDIN_FILENO);
 *    termio_runtime_sanitize();
 *    termio_drain_startup_noise();
 *    ... loop with termio_getch() ...
 *    termio_reset_tty(STDIN_FILENO);
 *
 * To enable detailed debug logging at build time:
 *      #define LIBTERMIO_ENABLE_DEBUG
 * before including this header (or pass -DLIBTERMIO_ENABLE_DEBUG in CFLAGS).
 *
 * At runtime, if built with LIBTERMIO_ENABLE_DEBUG, set:
 *      BRLEDIT_TERMIO_DEBUG=1           (logs to brledit_termio_debug.log)
 *      BRLEDIT_TERMIO_DEBUG=/path/file  (logs to chosen file, append mode)
 *
 * Without LIBTERMIO_ENABLE_DEBUG defined, all debug functions are inert
 * (compiled to no-ops) and introduce no overhead other than empty inline calls.
 */
/** @{ */
/** @file libtermio.h */

#ifndef LIBTERMIO_H
#define LIBTERMIO_H

#include "common.h"
#include <stddef.h>
#ifdef HAVE_SYS_TYPES_H
#  include "sys/types.h"
#endif

#ifdef __cplusplus
#define TERMIO_ZERO_INIT {}
#else
#define TERMIO_ZERO_INIT {0}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The following five functions are the lowest tier API and will work IFF one
 * of these flags is set:
 * HAVE_CONIO_H         use Windows console IO
 * HAVE_TERMIOS_H	use POXIX termios and tcsetattr() call with XOPEN flags
 * SYSV			use SysV Rel3 termio and TCSETA ioctl
 * BSD			use Version 7 / BSD sgttyb and TIOCSETP ioctl
 */
void clr_Echo(int fd);
void reset_Tty(int fd);
void save_Tty(int fd);
void set_Cbreak(int fd);
void set_Raw(int fd);

/* The more elaborate API is defined below - it will not (currently) work
 * on early legacy BSD systems, but handles more complex operations on
 * Windows consoles. */

enum termio_keycode {
    TERMIO_KEY_NONE = 0,
    TERMIO_KEY_ASCII = 256,
    TERMIO_KEY_UP,
    TERMIO_KEY_DOWN,
    TERMIO_KEY_LEFT,
    TERMIO_KEY_RIGHT,
    TERMIO_KEY_HOME,
    TERMIO_KEY_END,
    TERMIO_KEY_PGUP,
    TERMIO_KEY_PGDN,
    TERMIO_KEY_INSERT,
    TERMIO_KEY_DELETE,
    TERMIO_KEY_F1, TERMIO_KEY_F2, TERMIO_KEY_F3, TERMIO_KEY_F4,
    TERMIO_KEY_F5, TERMIO_KEY_F6, TERMIO_KEY_F7, TERMIO_KEY_F8,
    TERMIO_KEY_F9, TERMIO_KEY_F10, TERMIO_KEY_F11, TERMIO_KEY_F12,
    TERMIO_KEY_RESIZE
};

/* Core API */
void    termio_save_tty(int fd);
void    termio_reset_tty(int fd);

void    termio_set_raw(int fd);
void    termio_set_cbreak(int fd);
void    termio_set_cbreak_legacy(int fd);
void    termio_clear_echo(int fd);
int     termio_is_raw(int fd);

int     termio_get_winsize(int fd, int *rows, int *cols);
int     termio_poll_resize(int fd);
int     termio_getch(int fd);

void    termio_clear_screen(int fd);
void    termio_move_cursor(int fd, int row, int col);
void    termio_show_cursor(int fd, int show);
void    termio_flash_status_line(int fd, int row, int cols);
void    termio_beep(int fd);
ssize_t termio_write(int fd, const char *buf, size_t len);
void    termio_enable_vt_output(int fd_out);   /* no-op on POSIX */
void    termio_disable_mouse_reporting(int fd);

/* Optional helpers */
const char *termio_input_environment(void);
void        termio_runtime_sanitize(void);
void        termio_drain_startup_noise(void);
void        termio_detect_environment(void);

/* Debug helpers (no-op unless LIBTERMIO_ENABLE_DEBUG) */
void        termio_debug_init(void);
void        termio_debug_shutdown(void);


#if defined(LIBTERMIO_IMPLEMENTATION)

#ifdef HAVE_SYS_FILE_H
#  include <sys/file.h>
#endif

#ifdef HAVE_SYS_IOCTL_COMPAT_H
/* Needed only for very old BSD systems */
#  if defined(__FreeBSD__) && !defined(COMPAT_43TTY)
#    define COMPAT_43TTY 1
#  endif
#  include <sys/ioctl_compat.h>
#endif

#include "bio.h"

/*
 * This file will work IFF one of these three flags is set:
 * HAVE_CONIO_H         use Windows console IO
 * HAVE_TERMIOS_H	use POXIX termios and tcsetattr() call with XOPEN flags
 * SYSV			use SysV Rel3 termio and TCSETA ioctl
 * BSD			use Version 7 / BSD sgttyb and TIOCSETP ioctl
 */

#include <string.h>
#if defined(HAVE_MEMORY_H)
#  include <memory.h>
#endif

#if defined(HAVE_TERMIOS_H)
#  undef SYSV
#  undef BSD
#  include <termios.h>

static struct termios save_tio[FOPEN_MAX] = TERMIO_ZERO_INIT;
static struct termios curr_tio[FOPEN_MAX] = TERMIO_ZERO_INIT;

#else	/* !defined(HAVE_TERMIOS_H) */

#  ifdef SYSV
#    undef BSD
#    include <termio.h>
#    include <memory.h>
static struct termio save_tio[FOPEN_MAX] = TERMIO_ZERO_INIT;
static struct termio curr_tio[FOPEN_MAX] = TERMIO_ZERO_INIT;
#  endif /* SYSV */

#  ifdef BSD
#    undef SYSV
#    include <sys/ioctl.h>

static struct sgttyb save_tio[FOPEN_MAX] = TERMIO_ZERO_INIT;
static struct sgttyb curr_tio[FOPEN_MAX] = TERMIO_ZERO_INIT;
#  endif /* BSD */

#endif /* HAVE_TERMIOS_H */

#if defined(HAVE_CONIO_H)
#  include <conio.h>
/* array of unused function pointers, but created for consistency */
static int (*save_tio[FOPEN_MAX])(void) = TERMIO_ZERO_INIT;
static int (*curr_tio[FOPEN_MAX])(void) = TERMIO_ZERO_INIT;
#endif


static void
copy_Tio(
#if defined(BSD)
	struct sgttyb *to, struct sgttyb *from
#elif defined(SYSV)
	struct termio *to, struct termio *from
#elif defined(HAVE_TERMIOS_H)
	struct termios *to, struct termios*from
#elif defined(HAVE_CONIO_H)
	int (**to)(), int (**from)()
#endif
	)
{
    if (to && from)
	(void)memcpy((char *) to, (char *) from, sizeof(*from));
    return;
}


/*
   Clear echo mode, 'fd'.
*/
void
clr_Echo(int fd)
{
#ifdef BSD
    curr_tio[fd].sg_flags &= ~ECHO;		/* Echo mode OFF.	*/
    (void) ioctl(fd, TIOCSETP, &curr_tio[fd]);
#endif
#ifdef SYSV
    curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
    (void) ioctl(fd, TCSETA, &curr_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
    (void)tcsetattr(fd, TCSANOW, &curr_tio[fd]);
#endif
#ifdef HAVE_CONIO_H
    /* nothing to do */
#endif
    return;
}

/*
   Set the terminal back to the mode that the user had last time
   save_Tty() was called for 'fd'.
*/
void
reset_Tty(int fd)
{
#ifdef BSD
    (void) ioctl(fd, TIOCSETP, &save_tio[fd]); /* Write setting.		*/
#endif
#ifdef SYSV
    (void) ioctl(fd, TCSETA, &save_tio[fd]); /* Write setting.		*/
#endif
#ifdef HAVE_TERMIOS_H
    (void)tcsetattr(fd, TCSAFLUSH, &save_tio[fd]);
#endif
#ifdef HAVE_CONIO_H
    curr_tio[fd] = save_tio[fd];
 #endif
    return;
}

/*
   Get and save terminal parameters, 'fd'.
*/
void
save_Tty(int fd)
{
#ifdef BSD
    (void) ioctl(fd, TIOCGETP, &save_tio[fd]);
#endif
#ifdef SYSV
    (void) ioctl(fd, TCGETA, &save_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    (void)tcgetattr(fd, &save_tio[fd]);
#endif
#ifdef HAVE_CONIO_H
    /* nothing to do, 'cept copy */
#endif
    copy_Tio(&curr_tio[fd], &save_tio[fd]);
    return;
}

/*
   Set CBREAK mode, 'fd'.
*/
void
set_Cbreak(int fd)
{
#ifdef BSD
    curr_tio[fd].sg_flags |= CBREAK;	/* CBREAK mode ON.	*/
    (void) ioctl(fd, TIOCSETP, &curr_tio[fd]);
#endif
#ifdef SYSV
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void) ioctl(fd, TCSETA, &curr_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void)tcsetattr(fd, TCSANOW, &curr_tio[fd]);
#endif
#ifdef HAVE_CONIO_H
    /* concept doesn't exist */
#endif
    return;
}

/*
   Set raw mode, 'fd'.
*/
void
set_Raw(int fd)
{
#ifdef BSD
    curr_tio[fd].sg_flags |= RAW;		/* Raw mode ON.		*/
    (void) ioctl(fd, TIOCSETP, &curr_tio[fd]);
#endif
#ifdef SYSV
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void) ioctl(fd, TCSETA, &curr_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void)tcsetattr(fd, TCSANOW, &curr_tio[fd]);
#endif
#ifdef HAVE_CONIO_H
    /* nothing to do, raw is default */
#endif
    return;
}

/* ================= New API Implementation ================= */

#if defined(HAVE_WINDOWS_H)
#  define LIBTERMIO_WINDOWS 1
#else
#  define LIBTERMIO_POSIX 1
#endif

static const char TERMIO_RESET_SEQ[] = "\033[0m\033[?25h";

/* Shared helper (needed by both Windows and POSIX) */
static int termio_map_csi_number(int v){
    switch(v){
        case 1: case 7: return TERMIO_KEY_HOME;
        case 2: return TERMIO_KEY_INSERT;
        case 3: return TERMIO_KEY_DELETE;
        case 4: case 8: return TERMIO_KEY_END;
        case 5: return TERMIO_KEY_PGUP;
        case 6: return TERMIO_KEY_PGDN;
        case 11:return TERMIO_KEY_F1;
        case 12:return TERMIO_KEY_F2;
        case 13:return TERMIO_KEY_F3;
        case 14:return TERMIO_KEY_F4;
        case 15:return TERMIO_KEY_F5;
        case 17:return TERMIO_KEY_F6;
        case 18:return TERMIO_KEY_F7;
        case 19:return TERMIO_KEY_F8;
        case 20:return TERMIO_KEY_F9;
        case 21:return TERMIO_KEY_F10;
        case 23:return TERMIO_KEY_F11;
        case 24:return TERMIO_KEY_F12;
        default: return 0;
    }
}

/* -------------------------------------------------------------------------
 * WINDOWS IMPLEMENTATION
 * ------------------------------------------------------------------------- */
#ifdef LIBTERMIO_WINDOWS
#include <stdlib.h>
#include <string.h>

#if defined(__CYGWIN__) || defined(__MSYS__)
#  define LIBTERMIO_HAVE_TERMIOS_FALLBACK 1
#  include <termios.h>
#else
#  define LIBTERMIO_HAVE_TERMIOS_FALLBACK 0
#endif

/* Debug (optional) */
#ifdef LIBTERMIO_ENABLE_DEBUG
#  include <stdarg.h>
static int   termio_debug_enabled = 0;
static FILE *termio_debug_fp      = NULL;
static void termio_debug_printf(const char *fmt, ...) {
    if (!termio_debug_enabled || !termio_debug_fp) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(termio_debug_fp, fmt, ap);
    va_end(ap);
    fflush(termio_debug_fp);
}
#else
#  define termio_debug_printf(...) ((void)0)
#endif

/* State */
static HANDLE termio_hIn  = NULL;
static HANDLE termio_hOut = NULL;
static DWORD  termio_saved_in_mode  = 0;
static DWORD  termio_saved_out_mode = 0;
static int    termio_saved_valid    = 0;

static int termio_mode_console   = 0;
static int termio_mode_pipe      = 0;
static int termio_mode_mintty    = 0;
static int termio_mode_conpty    = 0;
static int termio_mode_stty_used = 0;
static int termio_vt_output_enabled = 0;
static int termio_win_resize_flag   = 0;

/* stty saved */
static char termio_stty_saved[256] = {0};

/* Pending drained keys (pipe mode) */
static unsigned char termio_pending_bytes[64];
static int termio_pending_len = 0;

/* Escape sequences to disable mouse/paste modes */
static const char *termio_disable_modes_seq =
    "\033[?1000l\033[?1001l\033[?1002l\033[?1003l"
    "\033[?1004l\033[?1005l\033[?1006l\033[?1015l\033[?1007l"
    "\033[?2004l";

#define TERMIO_ESC_BUF_MAX 128

/* Detect environment */
void termio_detect_environment(void) {
    if (!termio_hIn)
        termio_hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (!termio_hOut)
        termio_hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode;
    if (termio_hIn && GetConsoleMode(termio_hIn, &mode)) {
        termio_mode_console = 1;
    } else {
        if (termio_hIn) {
            DWORD ft = GetFileType(termio_hIn);
            if (ft == FILE_TYPE_PIPE)
                termio_mode_pipe = 1;
        }
    }
    const char *ms = getenv("MSYSTEM");
    const char *term = getenv("TERM");
    /* Tightened: only mark mintty if NOT a console handle */
    if (!termio_mode_console && (ms || (term && strstr(term,"xterm"))))
        termio_mode_mintty = 1;

    if (getenv("WT_SESSION") || getenv("ConEmuANSI") || getenv("ConEmuPID"))
        termio_mode_conpty = 1;
    if (termio_mode_console)
        termio_mode_conpty = 0;
}

/* Full write */
static ssize_t termio_full_write(int fd, const char *buf, size_t len) {
    (void)fd;
    if (!termio_hOut) termio_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!termio_hOut) return -1;
    size_t off = 0;
    while (off < len) {
        DWORD w=0;
        if (!WriteFile(termio_hOut, buf+off, (DWORD)(len-off), &w, NULL))
            return (off>0)?(ssize_t)off:-1;
        if (!w) break;
        off += w;
    }
    return (ssize_t)off;
}

static int termio_run_cmd(const char *cmd, char *outbuf, size_t outsz) {
    FILE *p = _popen(cmd, "r");
    if (!p) return 0;
    if (outbuf && outsz) {
        size_t n = fread(outbuf,1,outsz-1,p);
        outbuf[n]=0;
        while (n>0 && (outbuf[n-1]=='\n'||outbuf[n-1]=='\r')) outbuf[--n]=0;
    } else {
        char tmp[128];
        while (fread(tmp,1,sizeof(tmp),p) > 0) { }
    }
    int rc = _pclose(p);
    return (rc != -1);
}

static void termio_try_stty_enable(void) {
    if (!termio_mode_pipe || !termio_mode_mintty) return;
    if (termio_mode_console) return;
    if (termio_mode_stty_used) return;
    if (getenv("BRLEDIT_NO_STTY")) return;
    if (!termio_run_cmd("stty -g < /dev/tty", termio_stty_saved, sizeof(termio_stty_saved)))
        return;
    if (!*termio_stty_saved) return;
    if (!termio_run_cmd("stty raw -echo -icanon min 1 time 0 < /dev/tty", NULL, 0))
        return;
    termio_mode_stty_used = 1;
}

static void termio_try_stty_restore(void) {
    if (!termio_mode_stty_used) return;
    if (*termio_stty_saved) {
        char cmd[512];
        _snprintf(cmd,sizeof(cmd),"stty %s < /dev/tty", termio_stty_saved);
        termio_run_cmd(cmd,NULL,0);
    }
    termio_mode_stty_used=0;
    termio_stty_saved[0]=0;
}

/* Public env string */
const char *termio_input_environment(void) {
    if (termio_mode_console) return "WIN-CONSOLE";
    if (termio_mode_pipe && termio_mode_mintty && termio_mode_stty_used) return "PIPE-MINTTY-STTY";
    if (termio_mode_pipe && termio_mode_mintty) return "PIPE-MINTTY";
    if (termio_mode_pipe && termio_mode_conpty) return "PIPE-CONPTY";
    if (termio_mode_pipe) return "PIPE-WIN";
    return "UNKNOWN";
}

/* Save / Reset */
void termio_save_tty(int fd){
    (void)fd;
    termio_detect_environment();
    if (termio_mode_console) {
        if (termio_hIn)  GetConsoleMode(termio_hIn,  &termio_saved_in_mode);
        if (termio_hOut) GetConsoleMode(termio_hOut, &termio_saved_out_mode);
        termio_saved_valid = 1;
    }
}

void termio_reset_tty(int fd){
    (void)fd;
    termio_try_stty_restore();
    if (termio_saved_valid) {
        if (termio_hIn)  SetConsoleMode(termio_hIn,  termio_saved_in_mode);
        if (termio_hOut) SetConsoleMode(termio_hOut, termio_saved_out_mode);
    }
    termio_full_write(STDOUT_FILENO, TERMIO_RESET_SEQ, sizeof(TERMIO_RESET_SEQ)-1);
    termio_disable_mouse_reporting(STDOUT_FILENO);
    termio_win_resize_flag=0;
    termio_pending_len=0;
    termio_debug_shutdown();
}

void termio_enable_vt_output(int fd_out){
    (void)fd_out;
    if (!termio_hOut) termio_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!termio_hOut) return;
    DWORD mode;
    if (!GetConsoleMode(termio_hOut,&mode)) return;
    DWORD desired = mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(termio_hOut, desired))
        termio_vt_output_enabled = 1;
}

/* Enter raw console */
static void termio_enter_raw_console(void) {
    if (!termio_hIn) termio_hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (!termio_hIn) return;
    DWORD mode;
    if (!GetConsoleMode(termio_hIn,&mode)) return;
    termio_saved_in_mode = mode;
    DWORD newMode = mode;
    newMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
#ifdef ENABLE_QUICK_EDIT_MODE
    newMode |= ENABLE_EXTENDED_FLAGS;
    newMode &= ~ENABLE_QUICK_EDIT_MODE;
#endif
    newMode |= ENABLE_PROCESSED_INPUT;
#ifdef ENABLE_WINDOW_INPUT
    newMode |= ENABLE_WINDOW_INPUT;
#endif
#ifdef ENABLE_MOUSE_INPUT
    newMode &= ~ENABLE_MOUSE_INPUT;
#endif
#ifdef ENABLE_INSERT_MODE
    newMode &= ~ENABLE_INSERT_MODE;
#endif
    SetConsoleMode(termio_hIn,newMode);
    termio_enable_vt_output(STDOUT_FILENO);
}

void termio_set_cbreak_legacy(int fd){ (void)fd; termio_detect_environment(); if (termio_mode_console) termio_enter_raw_console(); else termio_try_stty_enable(); }
void termio_set_cbreak(int fd){ termio_set_cbreak_legacy(fd); }
void termio_set_raw(int fd){ termio_set_cbreak_legacy(fd); }

void termio_clear_echo(int fd){
    (void)fd;
    if (!termio_mode_console) return;
    DWORD mode;
    if (termio_hIn && GetConsoleMode(termio_hIn,&mode)) {
        mode &= ~ENABLE_ECHO_INPUT;
        SetConsoleMode(termio_hIn,mode);
    }
}

int termio_is_raw(int fd){
    (void)fd;
    if (termio_mode_stty_used) return 1;
    if (!termio_mode_console) return 0;
    DWORD mode;
    if (!termio_hIn || !GetConsoleMode(termio_hIn,&mode)) return 0;
    return ((mode & (ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT))==0);
}

int termio_get_winsize(int fd,int *rows,int *cols){
    (void)fd;
    if (!termio_hOut) termio_hOut=GetStdHandle(STD_OUTPUT_HANDLE);
    if (!termio_hOut) return -1;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(termio_hOut,&csbi)) return -1;
    if (rows) *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    if (cols) *cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
    return 0;
}

int termio_poll_resize(int fd){
    (void)fd;
    if (termio_win_resize_flag){
        termio_win_resize_flag=0;
        return 1;
    }
    return 0;
}

ssize_t termio_write(int fd,const char *buf,size_t len){
    (void)fd;
    return termio_full_write(STDOUT_FILENO, buf, len);
}

void termio_clear_screen(int fd){ (void)fd; termio_write(STDOUT_FILENO,"\033[2J\033[H",7); }
void termio_move_cursor(int fd,int row,int col){
    (void)fd;
    char seq[32]; int n=_snprintf(seq,sizeof(seq),"\033[%d;%dH",row,col);
    termio_write(STDOUT_FILENO,seq,(size_t)n);
}
void termio_show_cursor(int fd,int show){ (void)fd; termio_write(STDOUT_FILENO, show? "\033[?25h":"\033[?25l",6); }
void termio_flash_status_line(int fd,int row,int cols){
    (void)fd;
    char seq[48];
    int n=_snprintf(seq,sizeof(seq),"\033[%d;1H\033[7m",row);
    termio_write(STDOUT_FILENO,seq,(size_t)n);
    for (int i=0;i<cols;i++) termio_write(STDOUT_FILENO," ",1);
    termio_write(STDOUT_FILENO,"\033[0m",4);
    n=_snprintf(seq,sizeof(seq),"\033[%d;1H",row);
    termio_write(STDOUT_FILENO,seq,(size_t)n);
#ifdef _WIN32
    Sleep(60);
#endif
}
void termio_beep(int fd){ (void)fd; termio_write(STDOUT_FILENO,"\a",1); }

void termio_disable_mouse_reporting(int fd){
    (void)fd;
    termio_write(STDOUT_FILENO, termio_disable_modes_seq, strlen(termio_disable_modes_seq));
}

void termio_runtime_sanitize(void){
    termio_disable_mouse_reporting(STDOUT_FILENO);
}

/* Console event getch */
static int termio_win_read_console_key(void){
    INPUT_RECORD rec;
    DWORD rd;
    while (ReadConsoleInputW(termio_hIn,&rec,1,&rd)){
        if (!rd) continue;
        switch (rec.EventType){
            case WINDOW_BUFFER_SIZE_EVENT:
                termio_win_resize_flag=1;
                return TERMIO_KEY_RESIZE;
            case KEY_EVENT: {
                KEY_EVENT_RECORD *ke=&rec.Event.KeyEvent;
                if (!ke->bKeyDown) continue;
                WORD vk=ke->wVirtualKeyCode;
                WCHAR ch=ke->uChar.UnicodeChar;
                switch(vk){
                    case VK_UP: return TERMIO_KEY_UP;
                    case VK_DOWN: return TERMIO_KEY_DOWN;
                    case VK_LEFT: return TERMIO_KEY_LEFT;
                    case VK_RIGHT:return TERMIO_KEY_RIGHT;
                    case VK_HOME: return TERMIO_KEY_HOME;
                    case VK_END:  return TERMIO_KEY_END;
                    case VK_PRIOR:return TERMIO_KEY_PGUP;
                    case VK_NEXT: return TERMIO_KEY_PGDN;
                    case VK_INSERT:return TERMIO_KEY_INSERT;
                    case VK_DELETE:return TERMIO_KEY_DELETE;
                    case VK_F1: return TERMIO_KEY_F1;
                    case VK_F2: return TERMIO_KEY_F2;
                    case VK_F3: return TERMIO_KEY_F3;
                    case VK_F4: return TERMIO_KEY_F4;
                    case VK_F5: return TERMIO_KEY_F5;
                    case VK_F6: return TERMIO_KEY_F6;
                    case VK_F7: return TERMIO_KEY_F7;
                    case VK_F8: return TERMIO_KEY_F8;
                    case VK_F9: return TERMIO_KEY_F9;
                    case VK_F10:return TERMIO_KEY_F10;
                    case VK_F11:return TERMIO_KEY_F11;
                    case VK_F12:return TERMIO_KEY_F12;
                }
                if (ch>=1 && ch<128) return (int)ch;
                continue;
            }
            case MOUSE_EVENT:
            case FOCUS_EVENT:
            case MENU_EVENT:
            default:
                continue;
        }
    }
    return TERMIO_KEY_NONE;
}

/* Pipe mode ESC state machine */
typedef enum {
    TI_ESC_IDLE = 0,
    TI_ESC_SEEN,
    TI_ESC_CSI,
    TI_ESC_CSI_MOUSE_X10,
    TI_ESC_CSI_MOUSE_SGR,
    TI_ESC_CSI_COLLECT
} ti_esc_state_t;

static ti_esc_state_t ti_state=TI_ESC_IDLE;
static char ti_buf[TERMIO_ESC_BUF_MAX];
static int  ti_len=0;
static int  ti_discard_mouse=0;

static void ti_reset(void){
    ti_state=TI_ESC_IDLE;
    ti_len=0;
    ti_discard_mouse=0;
}

static int termio_pipe_read_byte(unsigned char *c){
    DWORD got=0;
    if (!ReadFile(termio_hIn,c,1,&got,NULL) || got!=1)
        return 0;
    return 1;
}

/* Returns >0 key; 0 ignore; -1 need more data */
static int ti_process_byte(unsigned char b){
    if (ti_discard_mouse){
        ti_buf[ti_len++]=(char)b;
        if (ti_state==TI_ESC_CSI_MOUSE_X10){
            if (ti_len>=6) ti_reset();
        } else if (ti_state==TI_ESC_CSI_MOUSE_SGR){
            if (b=='m'||b=='M'||ti_len>=TERMIO_ESC_BUF_MAX-1)
                ti_reset();
        }
        return 0;
    }

    switch(ti_state){
        case TI_ESC_IDLE:
            if (b==0x1B){ ti_state=TI_ESC_SEEN; ti_buf[0]=0x1B; ti_len=1; return -1; }
            if (b<0x20){
                switch(b){
                    case 0x04: case 0x08: case 0x09: case 0x0A:
                    case 0x0B: case 0x0D: case 0x0F: case 0x15:
                    case 0x16: case 0x17: case 0x18: case 0x19:
                    case 0x1F: return b;
                    default: return 0;
                }
            }
            if (b<128) return b;
            return '?';

        case TI_ESC_SEEN:
            ti_buf[ti_len++]=(char)b;
            if (b=='['){ ti_state=TI_ESC_CSI; return -1; }
            else if (b=='O'){ ti_state=TI_ESC_CSI_COLLECT; return -1; }
            else { ti_reset(); return 27; }

        case TI_ESC_CSI:
            ti_buf[ti_len++]=(char)b;
            if (b=='M'){ ti_state=TI_ESC_CSI_MOUSE_X10; ti_discard_mouse=1; return 0; }
            if (b=='<'){ ti_state=TI_ESC_CSI_MOUSE_SGR; ti_discard_mouse=1; return 0; }
            if ((b>='A'&&b<='Z')||(b>='a'&&b<='z')||b=='~'){
                int key=0;
                if (b=='A') key=TERMIO_KEY_UP;
                else if (b=='B') key=TERMIO_KEY_DOWN;
                else if (b=='C') key=TERMIO_KEY_RIGHT;
                else if (b=='D') key=TERMIO_KEY_LEFT;
                else if (b=='H') key=TERMIO_KEY_HOME;
                else if (b=='F') key=TERMIO_KEY_END;
                else if (b=='~'){
                    int val=0,ok=0;
                    for (int i=2;i<ti_len-1;i++){
                        if (ti_buf[i]>='0'&&ti_buf[i]<='9'){ val=val*10+(ti_buf[i]-'0'); ok=1; }
                        else if (ti_buf[i]==';') break;
                        else { ok=0; break; }
                    }
                    if (ok){
                        key=termio_map_csi_number(val);
                        if (val==200||val==201) key=0;
                    }
                }
                ti_reset();
                return key?key:0;
            } else {
                ti_state=TI_ESC_CSI_COLLECT;
                return -1;
            }

        case TI_ESC_CSI_COLLECT:
            ti_buf[ti_len++]=(char)b;
            if ((b>='A'&&b<='Z')||(b>='a'&&b<='z')||b=='~'){
                int key=0;
                if (b=='A') key=TERMIO_KEY_UP;
                else if (b=='B') key=TERMIO_KEY_DOWN;
                else if (b=='C') key=TERMIO_KEY_RIGHT;
                else if (b=='D') key=TERMIO_KEY_LEFT;
                else if (b=='H') key=TERMIO_KEY_HOME;
                else if (b=='F') key=TERMIO_KEY_END;
                else if (b=='~'){
                    int val=0,ok=0;
                    for (int i=2;i<ti_len-1;i++){
                        if (ti_buf[i]>='0'&&ti_buf[i]<='9'){ val=val*10+(ti_buf[i]-'0'); ok=1; }
                        else if (ti_buf[i]==';') break;
                        else { ok=0; break; }
                    }
                    if (ok){
                        key=termio_map_csi_number(val);
                        if (val==200||val==201) key=0;
                    }
                } else if (ti_buf[1]=='O'){
                    if (b=='P') key=TERMIO_KEY_F1;
                    else if (b=='Q') key=TERMIO_KEY_F2;
                    else if (b=='R') key=TERMIO_KEY_F3;
                    else if (b=='S') key=TERMIO_KEY_F4;
                }
                ti_reset();
                return key?key:0;
            }
            if (ti_len>=TERMIO_ESC_BUF_MAX-1) ti_reset();
            return -1;

        case TI_ESC_CSI_MOUSE_X10:
        case TI_ESC_CSI_MOUSE_SGR:
            return 0;
    }
    ti_reset();
    return 0;
}

/* Drain startup noise (pipe mode) */
void termio_drain_startup_noise(void){
    if (!termio_mode_pipe) return;
    if (!termio_hIn) termio_hIn=GetStdHandle(STD_INPUT_HANDLE);
    if (!termio_hIn) return;
    termio_pending_len=0;
    ti_reset();
    for (int iter=0; iter<64; ++iter){
        DWORD avail=0;
        if (!PeekNamedPipe(termio_hIn,NULL,0,NULL,&avail,NULL) || !avail)
            break;
        unsigned char b; DWORD g=0;
        if (!ReadFile(termio_hIn,&b,1,&g,NULL) || g!=1) break;
        int r=ti_process_byte(b);
        if (r>0 && termio_pending_len < (int)sizeof(termio_pending_bytes))
            termio_pending_bytes[termio_pending_len++]=(unsigned char)r;
    }
    ti_reset();
}

/* Pipe getch */
static int termio_win_vt_pipe_getch(void){
    if (termio_pending_len>0){
        int c=termio_pending_bytes[0];
        memmove(termio_pending_bytes, termio_pending_bytes+1, (size_t)termio_pending_len-1);
        termio_pending_len--;
        return c;
    }
    unsigned char b;
    for(;;){
        if (!termio_pipe_read_byte(&b))
            return TERMIO_KEY_NONE;
        int r=ti_process_byte(b);
        if (r==-1) continue;
        if (r==0)  continue;
        return r;
    }
}

int termio_getch(int fd){
    (void)fd;
    termio_detect_environment();
    if (termio_mode_console) return termio_win_read_console_key();
    return termio_win_vt_pipe_getch();
}

/* Debug init/shutdown (Windows) */
#ifdef LIBTERMIO_ENABLE_DEBUG
void termio_debug_init(void){
    if (termio_debug_enabled) return;
    const char *env=getenv("BRLEDIT_TERMIO_DEBUG");
    if (!env || !*env) return;
    const char *fname = (strncmp(env,"1", 2)==0) ? "brledit_termio_debug.log" : env;
    termio_debug_fp=fopen(fname,"ab");
    if (!termio_debug_fp) return;
    termio_debug_enabled=1;
    termio_detect_environment();
    termio_debug_printf("=== termio debug start ===\n");
    termio_debug_printf("ENV: %s (console=%d pipe=%d mintty=%d conpty=%d stty_used=%d)\n",
        termio_input_environment(), termio_mode_console, termio_mode_pipe,
        termio_mode_mintty, termio_mode_conpty, termio_mode_stty_used);
}
void termio_debug_shutdown(void){
    if (!termio_debug_enabled) return;
    termio_debug_printf("=== termio debug end ===\n");
    fclose(termio_debug_fp);
    termio_debug_fp=NULL;
    termio_debug_enabled=0;
}
#else
void termio_debug_init(void){}
void termio_debug_shutdown(void){}
#endif /* LIBTERMIO_ENABLE_DEBUG */

/* -------------------------------------------------------------------------
 * POSIX IMPLEMENTATION
 * ------------------------------------------------------------------------- */
#else /* LIBTERMIO_POSIX */

#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#ifdef HAVE_SYS_SIGNAL_H
#  include "sys/signal.h" // for SIGWINCH
#endif
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <errno.h>

/* No-op debug on POSIX unless explicitly enabled */
#ifdef LIBTERMIO_ENABLE_DEBUG
#  include <stdarg.h>
static int   termio_debug_enabled = 0;
static FILE *termio_debug_fp      = NULL;
static void termio_debug_printf(const char *fmt, ...) {
    if (!termio_debug_enabled || !termio_debug_fp) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(termio_debug_fp, fmt, ap);
    va_end(ap);
    fflush(termio_debug_fp);
}
void termio_debug_init(void){
    if (termio_debug_enabled) return;
    const char *env=getenv("BRLEDIT_TERMIO_DEBUG");
    if (!env||!*env) return;
    const char *fname=(strncmp(env,"1", 2)==0)?"brledit_termio_debug.log":env;
    termio_debug_fp=fopen(fname,"ab");
    if (!termio_debug_fp) return;
    termio_debug_enabled=1;
    termio_debug_printf("=== termio debug start (POSIX) ===\n");
}
void termio_debug_shutdown(void){
    if (!termio_debug_enabled) return;
    termio_debug_printf("=== termio debug end ===\n");
    fclose(termio_debug_fp);
    termio_debug_fp=NULL;
    termio_debug_enabled=0;
}
#else
#define termio_debug_printf(...) ((void)0)
void termio_debug_init(void){}
void termio_debug_shutdown(void){}
#endif

#define TERMIO_MAX_FD 512
static struct termios termio_saved_tio[TERMIO_MAX_FD];
static int termio_saved_flag[TERMIO_MAX_FD] = {0};
static volatile sig_atomic_t termio_sigwinch_flag = 0;

void termio_detect_environment(void){ (void)0; }

#ifdef SIGWINCH
static void termio_sigwinch_handler(int sig){ (void)sig; termio_sigwinch_flag=1; }
#endif

const char *termio_input_environment(void){ return "POSIX"; }

void termio_runtime_sanitize(void){ termio_disable_mouse_reporting(STDOUT_FILENO); }
void termio_drain_startup_noise(void){ (void)0; }

void termio_save_tty(int fd){
    if (fd < 0 || fd >= TERMIO_MAX_FD) return;
    if (tcgetattr(fd, &termio_saved_tio[fd]) == 0){
        termio_saved_flag[fd]=1;
#ifdef SIGWINCH
        struct sigaction sa; memset(&sa,0,sizeof(sa));
        sa.sa_handler=termio_sigwinch_handler;
        sigaction(SIGWINCH,&sa,NULL);
#endif
    }
}
void termio_reset_tty(int fd){
    if (fd < 0 || fd >= TERMIO_MAX_FD) return;
    if (termio_saved_flag[fd])
        tcsetattr(fd, TCSANOW, &termio_saved_tio[fd]);
    ssize_t w = write(STDOUT_FILENO, TERMIO_RESET_SEQ, sizeof(TERMIO_RESET_SEQ)-1);
    if (w < 0) {
	int e = errno;
	fprintf(stderr, "termio_reset_tty: write failed: %s\n", strerror(e));
    }
    termio_debug_shutdown();
}

static void termio_raw_posix(int fd){
    if (fd < 0 || fd >= TERMIO_MAX_FD) return;
    struct termios t;
    if (tcgetattr(fd,&t)!=0) return;
    t.c_lflag &= ~(ICANON|ECHO);
    t.c_iflag &= ~IXON;
    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(fd,TCSANOW,&t);
}
void termio_set_cbreak_legacy(int fd){ termio_raw_posix(fd); }
void termio_set_cbreak(int fd){ termio_raw_posix(fd); }
void termio_set_raw(int fd){ termio_raw_posix(fd); }
void termio_clear_echo(int fd){
    if (fd < 0 || fd >= TERMIO_MAX_FD) return;
    struct termios t; if (tcgetattr(fd,&t)!=0) return;
    t.c_lflag &= ~ECHO;
    tcsetattr(fd,TCSANOW,&t);
}
int termio_is_raw(int fd){
    if (fd < 0 || fd >= TERMIO_MAX_FD) return 0;
    struct termios t; if (tcgetattr(fd,&t)!=0) return 0;
    return ((t.c_lflag & (ICANON|ECHO))==0);
}
int termio_get_winsize(int fd,int *rows,int *cols){
    struct winsize ws;
    if (ioctl(fd,TIOCGWINSZ,&ws)==0){
        if (rows) *rows=ws.ws_row;
        if (cols) *cols=ws.ws_col;
        return 0;
    }
    return -1;
}
int termio_poll_resize(int fd){
    (void)fd;
    if (termio_sigwinch_flag){
        termio_sigwinch_flag=0;
        return 1;
    }
    return 0;
}
static ssize_t termio_full_write_posix(int fd,const char *buf,size_t len){
    size_t off=0;
    while (off<len){
        ssize_t r=write(fd,buf+off,len-off);
        if (r<0){
            if (errno==EINTR) continue;
            return (off>0)?(ssize_t)off:-1;
        }
        if (!r) break;
        off += (size_t)r;
    }
    return (ssize_t)off;
}
ssize_t termio_write(int fd,const char *buf,size_t len){
    return termio_full_write_posix(fd,buf,len);
}
void termio_clear_screen(int fd){ termio_write(fd,"\033[2J\033[H",7); }
void termio_move_cursor(int fd,int row,int col){
    char seq[32]; int n=snprintf(seq,sizeof(seq),"\033[%d;%dH",row,col);
    termio_write(fd,seq,(size_t)n);
}
void termio_show_cursor(int fd,int show){
    termio_write(fd, show? "\033[?25h":"\033[?25l",6);
}
void termio_flash_status_line(int fd,int row,int cols){
    char seq[48];
    int n=snprintf(seq,sizeof(seq),"\033[%d;1H\033[7m",row);
    termio_write(fd,seq,(size_t)n);
    for(int i=0;i<cols;i++) termio_write(fd," ",1);
    termio_write(fd,"\033[0m",4);
    n=snprintf(seq,sizeof(seq),"\033[%d;1H",row);
    termio_write(fd,seq,(size_t)n);
    struct timespec ts={0,60*1000000};
    nanosleep(&ts,NULL);
}
void termio_beep(int fd){ termio_write(fd,"\a",1); }
void termio_disable_mouse_reporting(int fd){
    (void)fd;
    const char *off =
        "\033[?1000l\033[?1001l\033[?1002l\033[?1003l"
        "\033[?1004l\033[?1005l\033[?1006l\033[?1015l\033[?1007l\033[?2004l";
    termio_write(STDOUT_FILENO,off,strlen(off));
}

/* POSIX no-op VT output enabler */
void termio_enable_vt_output(int fd_out){ (void)fd_out; }

/* POSIX getch with mouse/paste filtering */
int termio_getch(int fd){
    for(;;){
        unsigned char c;
        ssize_t r=read(fd,&c,1);
        if (r!=1) return TERMIO_KEY_NONE;
        if (c!=0x1B){
            if (c<0x20){
                switch(c){
                    case 0x04: case 0x08: case 0x09: case 0x0A:
                    case 0x0B: case 0x0D: case 0x0F: case 0x15:
                    case 0x16: case 0x17: case 0x18: case 0x19:
                    case 0x1F: return (int)c;
                    default: continue;
                }
            }
            if (c<128) return (int)c;
            return (int)'?';
        }
        unsigned char buf[128]; int len=0;
        fd_set rf; FD_ZERO(&rf); FD_SET(fd,&rf);
        struct timeval tv={0,20000};
        if (select(fd+1,&rf,NULL,NULL,&tv)<=0)
            return 0x1B;
        while (len < (int)sizeof(buf)-1){
            unsigned char b;
            if (read(fd,&b,1)!=1) break;
            buf[len++]=b;
            if (b=='~'||(b>='A'&&b<='Z')||(b>='a'&&b<='z')||b=='m'||b=='M')
                break;
            fd_set rf2; FD_ZERO(&rf2); FD_SET(fd,&rf2);
            struct timeval tv2={0,10000};
            if (select(fd+1,&rf2,NULL,NULL,&tv2)<=0)
                break;
        }
        char full[130]; full[0]=0x1B;
        for (int i=0;i<len;i++) full[i+1]=(char)buf[i];
        int flen=len+1;

        if (flen>=6 && full[1]=='[' && full[2]=='<' && (full[flen-1]=='m'||full[flen-1]=='M')) continue;
        if (flen==6 && full[1]=='[' && full[2]=='M') continue;
        if (full[flen-1]=='~'){
            int v=0,ok=1;
            for (int i=2;i<flen-1;i++){
                if (full[i]<'0'||full[i]>'9'){ ok=0; break; }
                v=v*10+(full[i]-'0');
            }
            if (ok && (v==200||v==201)) continue;
        }
        if (flen==3 && full[1]=='['){
            switch(full[2]){
                case 'A': return TERMIO_KEY_UP;
                case 'B': return TERMIO_KEY_DOWN;
                case 'C': return TERMIO_KEY_RIGHT;
                case 'D': return TERMIO_KEY_LEFT;
                case 'H': return TERMIO_KEY_HOME;
                case 'F': return TERMIO_KEY_END;
            }
        }
        if (full[flen-1]=='~'){
            int v=0,ok=0;
            for (int i=2;i<flen-1;i++){
                if (full[i]>='0'&&full[i]<='9'){ v=v*10+(full[i]-'0'); ok=1; }
                else if (full[i]==';') break;
                else { ok=0; break; }
            }
            if (ok){
                int m=termio_map_csi_number(v);
                if (v==200||v==201) m=0;
                if (m) return m;
            }
        }
        if (flen==3 && full[1]=='O'){
            switch(full[2]){
                case 'P': return TERMIO_KEY_F1;
                case 'Q': return TERMIO_KEY_F2;
                case 'R': return TERMIO_KEY_F3;
                case 'S': return TERMIO_KEY_F4;
                case 'H': return TERMIO_KEY_HOME;
                case 'F': return TERMIO_KEY_END;
            }
        }
        if (flen==1) return 0x1B;
    }
}

/* Debug stubs (POSIX release) handled above */

#endif /* POSIX vs WINDOWS */






#endif /* LIBTERMIO_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* LIBTERMIO_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
