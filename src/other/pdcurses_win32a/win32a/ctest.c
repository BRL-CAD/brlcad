#include <string.h>
#include "curses.h"

#define COLOR 17
#define COLOR_IDX 17

int main()
{
    char text[100];

    strcpy( text, "Hello world with dark gray text on black");
    initscr();
    start_color();
    use_default_colors( );
    if( init_pair(COLOR, COLOR_IDX, -1) == ERR) /* pair COLOR is a custom color on black */
        strcpy( text, "init_pair() failed");
    if( init_color(COLOR_IDX, 300, 300, 300) == ERR)  /* Color COLOR_IDX is now dark gray */
        strcat( text, "  init_color() failed");
    attron(COLOR_PAIR(COLOR));   /* Set color to our color pair */
    printw( text);
    getch();
    attroff(COLOR_PAIR(COLOR));  /* Turn off our color pair */
    endwin();
}
