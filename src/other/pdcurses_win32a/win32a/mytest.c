#include "curses.h"
#include <locale.h>

int main(int argc, char* argv[])
{
// setlocale(LC_ALL, "");

   initscr();
   endwin();
   return 0;
}

