/*

    File: intrfn.c

    Copyright (C) 1998-2009 Christophe GRENIER <grenier@cgsecurity.org>

    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#ifdef HAVE_NCURSES
#include <stdarg.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_CYGWIN_H
#include <sys/cygwin.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#include <errno.h>
#include "types.h"
#include "common.h"
#include "lang.h"
#include "intrf.h"
#include "intrfn.h"
#include "list.h"
#include "dir.h"
#include "log.h"
#include "hdaccess.h"
#include "autoset.h"

extern const arch_fnct_t arch_i386;
extern const arch_fnct_t arch_gpt;
extern const arch_fnct_t arch_mac;
extern const arch_fnct_t arch_none;
extern const arch_fnct_t arch_sun;
extern const arch_fnct_t arch_xbox;
extern char intr_buffer_screen[MAX_LINES][BUFFER_LINE_LENGTH+1];
extern int intr_nbr_line;

#define COLUMNS 		80
/* Use COLS (actual number of columns) or COLUMNS (number of columns the program has been designed for) ? */
#define INTER_DIR (LINES+16-25)
#define GS_DEFAULT -1
#define GS_key_ESCAPE -2

static int wmenuUpdate(WINDOW *window, const int yinfo, int y, int x, const struct MenuItem *menuItems, const unsigned int itemLength, const char *available, const int menuType, unsigned int current);
static int wgetch_nodelay(WINDOW *window);

int get_string(char *str, const int len, const char *def)
{
    int c;
    int i = 0;
    int x, y;
    int use_def = FALSE;
    curs_set(1);
    getyx(stdscr, y, x);
    wclrtoeol(stdscr);
    str[0] = 0;

    if (def != NULL) {
      mvwaddstr(stdscr,y, x, def);
      wmove(stdscr,y, x);
      use_def = TRUE;
    }

    wrefresh(stdscr);
    while ((c = wgetch(stdscr)) != '\n' && c != key_CR
#ifdef PADENTER
        && c!= PADENTER
#endif
        )
    {
      switch (c) {
        /* escape is generated by enter from keypad */
        /*
           case key_ESC:
           wmove(stdscr,y, x);
           wclrtoeol(stdscr);
           curs_set(0);
           wrefresh(stdscr);
           return GS_key_ESCAPE;
         */
        case KEY_DC:
        case KEY_BACKSPACE:
          if (i > 0) {
            str[--i] = 0;
            mvaddch(y, x+i, ' ');
            wmove(stdscr,y, x+i);
          } else if (use_def) {
            wclrtoeol(stdscr);
            use_def = FALSE;
          }
          break;
        default:
          if (i < len && isprint(c)) {
            mvaddch(y, x+i, c);
            if (use_def) {
              wclrtoeol(stdscr);
              use_def = FALSE;
            }
            str[i++] = c;
            str[i] = 0;
          }
      }
      wrefresh(stdscr);
    }
    curs_set(0);
    wrefresh(stdscr);
    if (use_def)
      return GS_DEFAULT;
    else
      return i;
}

static int wgetch_nodelay(WINDOW *window)
{
  int res;
  nodelay(window,TRUE);
  res=wgetch(window);
  nodelay(window,FALSE);
  return res;
}

/*
 * Actual function which prints the button bar and highlights the active button
 * Should not be called directly. Call function menuSelect instead.
 */

static int wmenuUpdate(WINDOW *window, const int yinfo, int y, int x, const struct MenuItem *menuItems, const unsigned int itemLength, const char *available, const int menuType, unsigned int current)
{
  unsigned int i, lmargin = x, ymargin = y;
  unsigned int lenNameMax=0;
  for( i = 0; menuItems[i].key!=0; i++ )
    if(strchr(available, menuItems[i].key)!=NULL )
    {
      unsigned int lenName = strlen( menuItems[i].name );
      if(lenNameMax<lenName && lenName < itemLength)
        lenNameMax=lenName;
    }
  /* Print available buttons */
  for( i = 0; menuItems[i].key!=0; i++ )
  {
    char buff[80];
    unsigned int lenName;
    const char *mi;
    wmove(window, y, x );
    wclrtoeol(window);

    /* Search next available button */
    while( menuItems[i].key!=0 && strchr(available, menuItems[i].key)==NULL )
    {
      i++;
    }
    if( menuItems[i].key==0 ) break; /* No more menu items */

    /* If selected item is not available and we have bypassed it,
       make current item selected */
    if( current < i && menuItems[current].key < 0 ) current = i;

    mi = menuItems[i].name;
    lenName = strlen( mi );
    if(lenName>=sizeof(buff))
    {
      log_critical("\nBUG: %s\n",mi);
    }
    if(lenName >= itemLength)
    {
      if( menuType & MENU_BUTTON )
        snprintf(buff, sizeof(buff),"[%s]",mi);
      else
        snprintf(buff, sizeof(buff),"%s",mi);
    }
    else
    {
      if( menuType & MENU_BUTTON )
      {
        if(menuType & MENU_VERT)
          snprintf( buff, sizeof(buff),"[%*s%-*s]", (itemLength - lenNameMax) / 2, "",
              (itemLength - lenNameMax + 1) / 2 + lenNameMax, mi );
        else
          snprintf( buff, sizeof(buff),"[%*s%-*s]", (itemLength - lenName) / 2, "",
              (itemLength - lenName + 1) / 2 + lenName, mi );
      }
      else
        snprintf( buff, sizeof(buff),"%*s%-*s", (itemLength - lenName) / 2, "",
            (itemLength - lenName + 1) / 2 + lenName, mi );
    }
    /* If current item is selected, highlight it */
    if( current == i )
    {
      wattrset(window, A_REVERSE);
    }

    /* Print item */
    mvwaddstr(window, y, x, buff );

    /* Lowlight after selected item */
    if( current == i )
    {
      wattroff(window, A_REVERSE);
    }
    if(menuType & MENU_VERT_WARN)
      mvwaddstr(window, y, x+itemLength+4, menuItems[i].desc);

    /* Calculate position for the next item */
    if( menuType & MENU_VERT )
    {
      y += 1;
      if( y >= yinfo - 1)
      {
        y = ymargin;
        x += (lenName < itemLength?itemLength:lenName) + MENU_SPACING;
        if( menuType & MENU_BUTTON ) x += 2;
      }
    }
    else
    {
      x += (lenName < itemLength?itemLength:lenName) + MENU_SPACING;
      if( menuType & MENU_BUTTON ) x += 2;
      if( x + lmargin + 12 > COLUMNS )
      {
        x = lmargin;
        y ++ ;
      }
    }
  }
  /* Print the description of selected item */
  if(!(menuType & MENU_VERT_WARN))
  {
    const char *mcd = menuItems[current].desc;
    mvwaddstr(window, yinfo, (COLUMNS - strlen( mcd )) / 2, mcd );
  }
  return y;
}

/* This function takes a list of menu items, lets the user choose one *
 * and returns the value keyboard shortcut of the selected menu item  */

int wmenuSelect(WINDOW *window, int yinfo, int y, int x, const struct MenuItem *menuItems, const unsigned int itemLength, const char *available, int menuType, unsigned int menuDefault)
{
  unsigned int current=menuDefault;
  return wmenuSelect_ext(window, yinfo, y, x, menuItems, itemLength, available, menuType, &current, NULL);
}

int wmenuSelect_ext(WINDOW *window, const int yinfo, int y, int x, const struct MenuItem *menuItems, const unsigned int itemLength, const char *available, int menuType, unsigned int *current, int *real_key)
{
  int i, ylast = y, key = 0;
  /*
     if( ( menuType & ( MENU_HORIZ | MENU_VERT ) )==0 )    
     {
     wprintw(window,"Menu without direction. Defaulting horizontal.");
     menuType |= MENU_HORIZ;
     }
   */
  /* Warning: current may be out of bound, not checked */
  /* Make sure that the current is one of the available items */
  while(strchr(available, menuItems[*current].key)==NULL)
  {
    (*current)++ ;
    if( menuItems[*current].key==0 )
    {
      *current = 0;
    }
  }
  /* Repeat until allowable choice has been made */
  while( key==0 )
  {
    /* Display the menu */
    ylast = wmenuUpdate( window, yinfo, y, x, menuItems, itemLength, available,
        menuType, *current );
    wrefresh(window);
    /* Don't put wgetch after the following wclrtoeol */
    key = wgetch(window);
    if(real_key!=NULL)
      *real_key=key;

    /* Clear out all prompts and such */
    for( i = y; i < ylast; i ++ )
    {
      wmove(window, i, x );
      wclrtoeol(window);
    }
    wmove(window, yinfo, 0 );
    wclrtoeol(window);
    if(strchr(available, key)==NULL)
    {
      if(key=='2')
	key=KEY_DOWN;
      else if(key=='4')
	key=KEY_LEFT;
      else if(key=='5')
	key=KEY_ENTER;
      else if(key=='6')
	key=KEY_RIGHT;
      else if(key=='8')
	key=KEY_UP;
    }
    /* Cursor keys */
    switch(key)
    {
      case KEY_UP:
        if( (menuType & MENU_VERT)!=0 )
        {
          do {
            if( (*current)-- == 0 )
            {
              while( menuItems[(*current)+1].key ) (*current) ++ ;
            }
          } while( strchr( available, menuItems[*current].key )==NULL );
          key = 0;
        }
        break;
      case KEY_DOWN:
        if( (menuType & MENU_VERT)!=0 )
        {
          do {
            (*current) ++ ;
            if( menuItems[*current].key==0 ) *current = 0 ;
          } while( strchr( available, menuItems[*current].key )==NULL );
          key = 0;
        }
        break;
      case KEY_RIGHT:
        if( (menuType & MENU_HORIZ)!=0 )
        {
          do {
            (*current) ++ ;
            if( menuItems[*current].key==0 ) 
            {
              *current = 0 ;
            }
          } while( strchr( available, menuItems[*current].key )==NULL );
          key = 0;
        }
        break;
      case KEY_LEFT:
        if( (menuType & MENU_HORIZ) !=0)
        {
          do {
            if( (*current)-- == 0 )
            {
              while( menuItems[(*current) + 1].key ) (*current) ++ ;
            }
          } while( strchr( available, menuItems[*current].key )==NULL );
          key = 0;
        }
        break;
    }
    /* Enter equals to the keyboard shortcut of current menu item */
    if((key==13) || (key==10) || (key==KEY_ENTER) ||
        (((menuType & MENU_VERT) != 0) && ((menuType & MENU_VERT_ARROW2VALID) != 0)
         && (key==KEY_RIGHT || key==KEY_LEFT)))
      key = menuItems[*current].key;
#ifdef PADENTER
    if(key==PADENTER)
      key = menuItems[*current].key;
#endif

    /* Is pressed key among acceptable ones */
    if( key!=0 && (strchr(available, toupper(key))!=NULL || strchr(available, key)!=NULL))
      break;
    /* Should all keys to be accepted? */
    if( key && (menuType & MENU_ACCEPT_OTHERS)!=0 ) break;
    /* The key has not been accepted so far -> let's reject it */
#ifdef DEBUG
    if( key )
    {
      wmove(window,5,0);
      wprintw(window,"key %03X",key);
      putchar( BELL );
    }
#endif
    key = 0;
  }
  /* Clear out prompts and such */
  for( i = y; i <= ylast; i ++ )
  {
    wmove(window, i, x );
    wclrtoeol(window);
  }
  wmove(window, yinfo, 0 );
  wclrtoeol(window);
  return key;
}

/* Function menuSelect takes way too many parameters  *
 * Luckily, most of time we can do with this function */

int wmenuSimple(WINDOW *window,const struct MenuItem *menuItems, unsigned int menuDefault)
{
    unsigned int i, j, itemLength = 0;
    char available[MENU_MAX_ITEMS];

    for(i = 0; menuItems[i].key; i++)
    {
      j = strlen(menuItems[i].name);
      if( j > itemLength ) itemLength = j;
      available[i] = menuItems[i].key;
    }
    available[i] = 0;
    return wmenuSelect(window, 23, 18, 0, menuItems, itemLength, available, MENU_HORIZ | MENU_BUTTON, menuDefault);
}

/* End of command menu support code */

unsigned long long int ask_number(const unsigned long long int val_cur, const unsigned long long int val_min, const unsigned long long int val_max, const char * _format, ...)
{
  char res[200];
  char res2[200];
  char response[128];
  char def[128];
  unsigned long int tmp_val;
  va_list ap;
  va_start(ap,_format);
  vsnprintf(res,sizeof(res),_format,ap);
  if(val_min!=val_max)
    snprintf(res2,sizeof(res2),"(%llu-%llu) :",val_min,val_max);
  else
    res2[0]='\0';
  va_end(ap);
  waddstr(stdscr, res);
  waddstr(stdscr, res2);
  sprintf(def, "%llu", val_cur);
  if (get_string(response, sizeof(response), def) > 0)
  {
#ifdef HAVE_ATOLL
    tmp_val = atoll(response);
#else
    tmp_val = atol(response);
#endif
    if (val_min==val_max || (tmp_val >= val_min && tmp_val <= val_max))
      return tmp_val;
  }
  return val_cur;
}

void dump_ncurses(const void *nom_dump, unsigned int lng)
{
  WINDOW *window=newwin(0,0,0,0);	/* full screen */
  keypad(window, TRUE); /* Need it to get arrow key */
  aff_copy(window);
  dump(window, nom_dump, lng);
  dump_log(nom_dump,lng);
  delwin(window);
  (void) clearok(stdscr, TRUE);
#ifdef HAVE_TOUCHWIN
  touchwin(stdscr);
#endif
}
#define DUMP_MAX_LINES		(LINES+15-25)
#define DUMP_X			0
#define DUMP_Y			7
#define INTER_DUMP_X		DUMP_X
#define INTER_DUMP_Y		(LINES+23-25)

void dump(WINDOW *window, const void *nom_dump,unsigned int lng)
{
  unsigned int nbr_line;
  unsigned int pos=0;
  int done=0;
  unsigned int menu=2;   /* default : quit */
  const char *options="PNQ";
  struct MenuItem menuDump[]=
  {
    { 'P', "Previous",""},
    { 'N', "Next","" },
    { 'Q',"Quit","Quit dump section"},
    { 0, NULL, NULL }
  };
  nbr_line=(lng+0x10-1)/0x10;
  if(nbr_line<=DUMP_MAX_LINES)
  {
    options="Q";
  }
  /* ncurses interface */
  mvwaddstr(window,DUMP_Y,DUMP_X,msg_DUMP_HEXA);
  /* On pourrait utiliser wscrl */
  do
  {
    unsigned char car;
    unsigned int i,j;
    for (i=pos; (i<nbr_line)&&((i-pos)<DUMP_MAX_LINES); i++)
    {
      wmove(window,DUMP_Y+i-pos,DUMP_X);
      wclrtoeol(window);
      wprintw(window,"%04X ",i*0x10);
      for(j=0; j< 0x10;j++)
      {
        if(i*0x10+j<lng)
        {
          car=*((const unsigned char*)nom_dump+i*0x10+j);
          wprintw(window,"%02x", car);
        }
        else
          wprintw(window,"  ");
        if(j%4==(4-1))
          wprintw(window," ");
      }
      wprintw(window,"  ");
      for(j=0; j< 0x10;j++)
      {
        if(i*0x10+j<lng)
        {
          car=*((const unsigned char*)nom_dump+i*0x10+j);
          if ((car<32)||(car >= 127))
            wprintw(window,".");
          else
            wprintw(window,"%c",  car);
        }
        else
          wprintw(window," ");
      }
    }
    switch (wmenuSelect(window, INTER_DUMP_Y+1, INTER_DUMP_Y, INTER_DUMP_X, menuDump, 8, options, MENU_HORIZ | MENU_BUTTON | MENU_ACCEPT_OTHERS, menu))
    {
      case 'p':
      case 'P':
      case KEY_UP:
        if(strchr(options,'N')!=NULL)
        {
          menu=0;
          if(pos>0)
            pos--;
        }
        break;
      case 'n':
      case 'N':
      case KEY_DOWN:
        if(strchr(options,'N')!=NULL)
        {
          menu=1;
          if(pos<nbr_line-DUMP_MAX_LINES)
            pos++;
        }
        break;
      case KEY_PPAGE:
        if(strchr(options,'N')!=NULL)
        {
          menu=0;
          if(pos>DUMP_MAX_LINES-1)
            pos-=DUMP_MAX_LINES-1;
          else
            pos=0;
        }
        break;
      case KEY_NPAGE:
        if(strchr(options,'N')!=NULL)
        {
          menu=1;
          if(pos<nbr_line-DUMP_MAX_LINES-(DUMP_MAX_LINES-1))
            pos+=DUMP_MAX_LINES-1;
          else
            pos=nbr_line-DUMP_MAX_LINES;
        }
        break;
      case key_ESC:
      case 'q':
      case 'Q':
        done = TRUE;
        break;
    }
  } while(done==FALSE);
}

void dump2(WINDOW *window, const void *dump_1, const void *dump_2, const unsigned int lng)
{
  unsigned int nbr_line;
  unsigned int pos=0;
  int done=0;
  unsigned int menu=2;   /* default : quit */
  const char *options="PNQ";
  struct MenuItem menuDump[]=
  {
    { 'P', "Previous",""},
    { 'N', "Next","" },
    { 'Q',"Quit","Quit dump section"},
    { 0, NULL, NULL }
  };
  /* ncurses interface */
  nbr_line=(lng+0x08-1)/0x08;
  if(nbr_line<=DUMP_MAX_LINES)
  {
    options="Q";
  }
  do
  {
    unsigned int i,j;
    for (i=pos; (i<nbr_line)&&((i-pos)<DUMP_MAX_LINES); i++)
    {
      wmove(window,DUMP_Y+i-pos,DUMP_X);
      wclrtoeol(window);
      wprintw(window,"%04X ",i*0x08);
      for(j=0; j<0x08;j++)
      {
        if(i*0x08+j<lng)
        {
          unsigned char car1=*((const unsigned char*)dump_1+i*0x08+j);
          unsigned char car2=*((const unsigned char*)dump_2+i*0x08+j);
          if(car1!=car2)
            wattrset(window, A_REVERSE);
          wprintw(window,"%02x", car1);
          if(car1!=car2)
            wattroff(window, A_REVERSE);
        }
        else
          wprintw(window," ");
        if(j%4==(4-1))
          wprintw(window," ");
      }
      wprintw(window,"  ");
      for(j=0; j<0x08;j++)
      {
        if(i*0x08+j<lng)
        {
          unsigned char car1=*((const unsigned char*)dump_1+i*0x08+j);
          unsigned char car2=*((const unsigned char*)dump_2+i*0x08+j);
          if(car1!=car2)
            wattrset(window, A_REVERSE);
          if ((car1<32)||(car1 >= 127))
            wprintw(window,".");
          else
            wprintw(window,"%c",  car1);
          if(car1!=car2)
            wattroff(window, A_REVERSE);
        }
        else
          wprintw(window," ");
      }
      wprintw(window,"  ");
      for(j=0; j<0x08;j++)
      {
        if(i*0x08+j<lng)
        {
          unsigned char car1=*((const unsigned char*)dump_1+i*0x08+j);
          unsigned char car2=*((const unsigned char*)dump_2+i*0x08+j);
          if(car1!=car2)
            wattrset(window, A_REVERSE);
          wprintw(window,"%02x", car2);
          if(car1!=car2)
            wattroff(window, A_REVERSE);
          if(j%4==(4-1))
            wprintw(window," ");
        }
        else
          wprintw(window," ");
      }
      wprintw(window,"  ");
      for(j=0; j<0x08;j++)
      {
        if(i*0x08+j<lng)
        {
          unsigned char car1=*((const unsigned char*)dump_1+i*0x08+j);
          unsigned char car2=*((const unsigned char*)dump_2+i*0x08+j);
          if(car1!=car2)
            wattrset(window, A_REVERSE);
          if ((car2<32)||(car2 >= 127))
            wprintw(window,".");
          else
            wprintw(window,"%c",  car2);
          if(car1!=car2)
            wattroff(window, A_REVERSE);
        }
        else
          wprintw(window," ");
      }
    }
    switch (wmenuSelect(window, INTER_DUMP_Y+1, INTER_DUMP_Y, INTER_DUMP_X, menuDump, 8, options, MENU_HORIZ | MENU_BUTTON | MENU_ACCEPT_OTHERS, menu))
    {
      case 'p':
      case 'P':
      case KEY_UP:
        if(strchr(options,'N')!=NULL)
        {
          menu=0;
          if(pos>0)
            pos--;
        }
        break;
      case 'n':
      case 'N':
      case KEY_DOWN:
        if(strchr(options,'N')!=NULL)
        {
          menu=1;
          if(pos<nbr_line-DUMP_MAX_LINES)
            pos++;
        }
        break;
      case KEY_PPAGE:
        if(strchr(options,'N')!=NULL)
        {
          menu=0;
          if(pos>DUMP_MAX_LINES-1)
            pos-=DUMP_MAX_LINES-1;
          else
            pos=0;
        }
        break;
      case KEY_NPAGE:
        if(strchr(options,'N')!=NULL)
        {
          menu=1;
          if(pos<nbr_line-DUMP_MAX_LINES-(DUMP_MAX_LINES-1))
            pos+=DUMP_MAX_LINES-1;
          else
            pos=nbr_line-DUMP_MAX_LINES;
        }
        break;
      case key_ESC:
      case 'q':
      case 'Q':
        done = TRUE;
        break;
    }
  } while(done==FALSE);
}

int screen_buffer_display(WINDOW *window, const char *options_org, const struct MenuItem *menuItems)
{
  unsigned int menu=0;
  return screen_buffer_display_ext(window,options_org,menuItems,&menu);
}

#define INTER_ANALYSE_X		0
#define INTER_ANALYSE_Y 	8
#define INTER_ANALYSE_MENU_X 	0
#define INTER_ANALYSE_MENU_Y 	(LINES-2)
#define INTER_MAX_LINES 	(INTER_ANALYSE_MENU_Y-INTER_ANALYSE_Y-2)
int screen_buffer_display_ext(WINDOW *window, const char *options_org, const struct MenuItem *menuItems, unsigned int *menu)
{
  int first_line_to_display=0;
  int current_line=0;
  int done=0;
  char options[20];
  struct MenuItem menuDefault[]=
  {
    { 'P', "Previous",""},
    { 'N', "Next","" },
    { 'Q', "Quit","Quit this section"},
    { 0, NULL, NULL }
  };
  const unsigned int itemLength=8;
  /* FIXME itemLength */
  strncpy(options,"Q",sizeof(options));
  strncat(options,options_org,sizeof(options)-strlen(options));
  if(intr_buffer_screen[intr_nbr_line][0]!='\0')
    intr_nbr_line++;
  /* curses interface */
  do
  {
    int i;
    int key;
    wmove(window, INTER_ANALYSE_Y-1, INTER_ANALYSE_X+4);
    wclrtoeol(window);
    if(first_line_to_display>0)
      wprintw(window, "Previous");
    if(intr_nbr_line>INTER_MAX_LINES && has_colors())
    {
      for (i=first_line_to_display; i<intr_nbr_line && (i-first_line_to_display)<INTER_MAX_LINES; i++)
      {
	wmove(window,INTER_ANALYSE_Y+i-first_line_to_display,INTER_ANALYSE_X);
	wclrtoeol(window);
	if(i==current_line)
	  wattrset(window, A_REVERSE);
	wprintw(window, "%-*s", COLS, intr_buffer_screen[i]);
	if(i==current_line)
	  wattroff(window, A_REVERSE);
      }
    }
    else
    {
      for (i=first_line_to_display; i<intr_nbr_line && (i-first_line_to_display)<INTER_MAX_LINES; i++)
      {
	wmove(window,INTER_ANALYSE_Y+i-first_line_to_display,INTER_ANALYSE_X);
	wclrtoeol(window);
	wprintw(window, "%-*s", COLS, intr_buffer_screen[i]);
      }
    }
    wmove(window, INTER_ANALYSE_Y+INTER_MAX_LINES, INTER_ANALYSE_X+4);
    wclrtoeol(window);
    if(i<intr_nbr_line)
      wprintw(window, "Next");
    key=wmenuSelect_ext(window, INTER_ANALYSE_MENU_Y+1,
	INTER_ANALYSE_MENU_Y, INTER_ANALYSE_MENU_X,
	(menuItems!=NULL?menuItems:menuDefault), itemLength, options,
	MENU_HORIZ | MENU_BUTTON | MENU_ACCEPT_OTHERS, menu,NULL);
    switch (key)
    {
      case key_ESC:
      case 'q':
      case 'Q':
        done = TRUE;
        break;
      case 'p':
      case 'P':
      case KEY_UP:
        if(current_line>0)
          current_line--;
        break;
      case 'n':
      case 'N':
      case KEY_DOWN:
        if(current_line<intr_nbr_line-1)
          current_line++;
        break;
      case KEY_PPAGE:
        if(current_line>INTER_MAX_LINES-1)
          current_line-=INTER_MAX_LINES-1;
        else
          current_line=0;
        break;
      case KEY_NPAGE:
        if(current_line+INTER_MAX_LINES-1 < intr_nbr_line-1)
          current_line+=INTER_MAX_LINES-1;
        else
          current_line=intr_nbr_line-1;
        break;
      default:
        if(strchr(options,toupper(key))!=NULL)
          return toupper(key);
        break;
    }
    if(current_line<first_line_to_display)
      first_line_to_display=current_line;
    if(current_line>=first_line_to_display+INTER_MAX_LINES)
      first_line_to_display=current_line-INTER_MAX_LINES+1;
  } while(done!=TRUE);
  return 0;
}

void aff_part(WINDOW *window,const unsigned int newline,const disk_t *disk_car,const partition_t *partition)
{
  const char *msg;
  msg=aff_part_aux(newline, disk_car, partition);
  wprintw(window,"%s",msg);
}

void aff_LBA2CHS(const disk_t *disk_car, const unsigned long int pos_LBA)
{
  unsigned long int tmp;
  unsigned long int cylinder, head, sector;
  tmp=disk_car->geom.sectors_per_head;
  sector=(pos_LBA%tmp)+1;
  tmp=pos_LBA/tmp;
  cylinder=tmp / disk_car->geom.heads_per_cylinder;
  head=tmp % disk_car->geom.heads_per_cylinder;
  wprintw(stdscr, "%lu/%lu/%lu", cylinder, head, sector);
}

int ask_YN(WINDOW *window)
{
  char res;
  curs_set(1);
  wrefresh(window);
  do
  {
    res=toupper(wgetch(window));
  } while((res!=c_NO)&&(res!=c_YES));
  curs_set(0);
  wprintw(window,"%c\n",res);
  return (res==c_YES);
}

int ask_confirmation(const char*_format, ...)
{
  va_list ap;
  int res;
  WINDOW *window=newwin(0,0,0,0);	/* full screen */
  aff_copy(window);
  va_start(ap,_format);
  vaff_txt(4, window, _format, ap);
  va_end(ap);
  res=ask_YN(window);
  delwin(window);
  (void) clearok(stdscr, TRUE);
#ifdef HAVE_TOUCHWIN
  touchwin(stdscr);
#endif
  return res;
}

void not_implemented(const char *msg)
{
  WINDOW *window=newwin(0,0,0,0);	/* full screen */
  aff_copy(window);
  wmove(window,7,0);
  wprintw(window,"Function %s not implemented",msg);
  log_warning("Function %s not implemented\n",msg);
  wmove(window,22,0);
  wattrset(window, A_REVERSE);
  wprintw(window,"[ Abort ]");
  wattroff(window, A_REVERSE);
  wrefresh(window);
  while(wgetch(window)==ERR);
  delwin(window);
  (void) clearok(stdscr, TRUE);
#ifdef HAVE_TOUCHWIN
  touchwin(stdscr);
#endif
}

#if defined(DJGPP) || defined(__MINGW32__)
#else
static SCREEN *screenp=NULL;
#endif

static char *filename_to_directory(const char *filename)
{
  char buf[2048];
  char *res;
#ifdef HAVE_READLINK
  int len;
  len=readlink(filename,buf,sizeof(buf)-1);
  if(len>=0)
    buf[len]='\0';
  else
  {
    strncpy(buf,filename,sizeof(buf)-1);
    buf[sizeof(buf)-1]='\0';
  }
#else
  strncpy(buf,filename,sizeof(buf)-1);
  buf[sizeof(buf)-1]='\0';
#endif
  res=dirname(buf);
  if(res==NULL)
    return NULL;
#ifdef HAVE_GETCWD
  if(strcmp(res,".")==0 && getcwd(buf, sizeof(buf)-1)!=NULL)
  {
    buf[sizeof(buf)-1]='\0';
    res=buf;
  }
#endif
#ifdef __CYGWIN__
  {
    char beautifull_dst_directory[2048];
    cygwin_conv_to_win32_path(res, beautifull_dst_directory);
    return strdup(beautifull_dst_directory);
  }
#else
  return strdup(res);
#endif
}

int start_ncurses(const char *prog_name, const char *real_prog_name)
{
#if defined(DJGPP) || defined(__MINGW32__)
  if(initscr()==NULL)
  {
    log_critical("initscr() has failed. Exiting\n");
    printf("initscr() has failed. Exiting\n");
    printf("Press Enter key to quit.\n");
    getchar();
    return 1;
  }
#else
  {
    int term_overwrite;
    char *terminfo=filename_to_directory(real_prog_name);
    for(term_overwrite=0;screenp==NULL && term_overwrite<=1;term_overwrite++)
    {
#ifdef HAVE_SETENV
#if defined(TARGET_BSD)
      setenv("TERM","cons25",term_overwrite);
#elif defined(TARGET_LINUX)
      setenv("TERM","linux",term_overwrite);
#elif defined(__CYGWIN__)
      setenv("TERM","cygwin",term_overwrite);
#elif defined(__OS2__)
      setenv("TERM","ansi",term_overwrite);
#elif defined(__APPLE__)
      setenv("TERM","xterm-color",term_overwrite);
#endif
#endif
      screenp=newterm(NULL,stdout,stdin);
#ifdef HAVE_SETENV
      if(screenp==NULL && terminfo!=NULL)
      {
        setenv("TERMINFO", terminfo, 1);
        screenp=newterm(NULL,stdout,stdin);
      }
      if(screenp==NULL)
      {
        setenv("TERMINFO",".",1);
        screenp=newterm(NULL,stdout,stdin);
      }
      if(screenp==NULL)
        unsetenv("TERMINFO");
#endif
    }
    if(screenp==NULL)
    {
      log_critical("Terminfo file is missing.\n");
#if defined(__CYGWIN__)
      printf("The terminfo file '%s\\c\\cygwin' is missing.\n", terminfo);
#else
      printf("Terminfo file is missing.\n");
#endif
      printf("Extract all files and subdirectories before running the program.\n");
      printf("Press Enter key to quit.\n");
      getchar();
      free(terminfo);
      return 1;
    }
    free(terminfo);
  }
#endif
  noecho();
#ifndef DJGPP
  nonl(); /*don't use for Dos version but enter will work with it... dilema */
#endif
  /*  intrflush(stdscr, FALSE); */
  cbreak();
  /* Should solve a problem with users who redefined the colors */
  if(has_colors())
  {
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
  }
  curs_set(0);
  {
    int quit=0;
    while(LINES>=8 && LINES<24 && quit==0)
    {
      aff_copy(stdscr);
      wmove(stdscr,4,0);
      wprintw(stdscr,"%s need 24 lines to work.", prog_name);
      wmove(stdscr,5,0);
      wprintw(stdscr,"Please enlarge the terminal.");
      wmove(stdscr,LINES-2,0);
      wattrset(stdscr, A_REVERSE);
      wprintw(stdscr,"[ Quit ]");
      wattroff(stdscr, A_REVERSE);
      wrefresh(stdscr);
      switch(wgetch(stdscr))
      {
	case 'q':
	case 'Q':
	case KEY_ENTER:
#ifdef PADENTER
	case PADENTER:
#endif
	case '\n':
	case '\r':
	  quit=1;
	  break;
      }
    }
  }
  if(LINES<24)
  {
    end_ncurses();
    printf("%s need 24 lines to work.\nPlease enlarge the terminal and restart %s.\n",prog_name,prog_name);
    log_critical("Terminal has only %d lines\n",LINES);
    return 1;
  }
  return 0;
}

int end_ncurses()
{
  wclear(stdscr);
  wrefresh(stdscr);
  nl();
  endwin();
#if defined(DJGPP) || defined(__MINGW32__)
#else
#ifdef HAVE_DELSCREEN
  if(screenp!=NULL)
    delscreen(screenp);
#endif
#endif
  return 0;
}

char *ask_log_location(const char*filename)
{
  static char response[128];
  aff_copy(stdscr);
  if(filename!=NULL)
  {
    wmove(stdscr,6,0);
    wprintw(stdscr,"Cannot open %s: %s\n",filename, strerror(errno));
  }
  wmove(stdscr,8,0);
  wprintw(stdscr,"Please enter the full log filename or press ");
  if(has_colors())
    wbkgdset(stdscr,' ' | A_BOLD | COLOR_PAIR(0));
  wprintw(stdscr,"Enter");
  if(has_colors())
    wbkgdset(stdscr,' ' | COLOR_PAIR(0));
  wmove(stdscr,9,0);
  wprintw(stdscr,"to abort log file creation.\n");
  if (get_string(response, sizeof(response), NULL) > 0)
    return response;
  return NULL;
}

int check_enter_key_or_s(WINDOW *window)
{
  switch(wgetch_nodelay(window))
  {
    case KEY_ENTER:
#ifdef PADENTER
    case PADENTER:
#endif
    case '\n':
    case '\r':
    case 's':
    case 'S':
      return 1;
  }
  return 0;
}

int interface_partition_type_ncurses(disk_t *disk_car)
{
  /* arch_list must match the order from menuOptions */
  const arch_fnct_t *arch_list[]={&arch_i386, &arch_gpt, &arch_mac, &arch_none, &arch_sun, &arch_xbox, NULL};
  unsigned int menu;
  for(menu=0;arch_list[menu]!=NULL && disk_car->arch!=arch_list[menu];menu++);
  if(arch_list[menu]==NULL)
  {
    menu=0;
    disk_car->arch=arch_list[menu];
  }
  /* ncurses interface */
  {
    int car;
    int real_key;
    struct MenuItem menuOptions[]=
    {
      { 'I', arch_i386.part_name, "Intel/PC partition" },
      { 'G', arch_gpt.part_name, "EFI GPT partition map (Mac i386, some x86_64...)" },
      { 'M', arch_mac.part_name, "Apple partition map" },
      { 'N', arch_none.part_name, "Non partitioned media" },
      { 'S', arch_sun.part_name, "Sun Solaris partition"},
      { 'X', arch_xbox.part_name, "XBox partition"},
      { 'Q', "Return", "Return to disk selection"},
      { 0, NULL, NULL }
    };
    aff_copy(stdscr);
    wmove(stdscr,5,0);
    wprintw(stdscr,"%s\n",disk_car->description_short(disk_car));
    wmove(stdscr,INTER_PARTITION_Y-1,0);
    wprintw(stdscr,"Please select the partition table type, press Enter when done.");
    wmove(stdscr,20,0);
    wprintw(stdscr,"Note: Do NOT select 'None' for media with only a single partition. It's very");
    wmove(stdscr,21,0);
    wprintw(stdscr,"rare for a drive to be 'Non-partitioned'.");
    car=wmenuSelect_ext(stdscr, 23, INTER_PARTITION_Y, INTER_PARTITION_X, menuOptions, 7, "IGMNSXQ", MENU_BUTTON | MENU_VERT | MENU_VERT_WARN, &menu,&real_key);
    switch(car)
    {
      case 'i':
      case 'I':
        disk_car->arch=&arch_i386;
        break;
      case 'g':
      case 'G':
        disk_car->arch=&arch_gpt;
        break;
      case 'm':
      case 'M':
        disk_car->arch=&arch_mac;
        break;
      case 'n':
      case 'N':
        disk_car->arch=&arch_none;
        break;
      case 's':
      case 'S':
        disk_car->arch=&arch_sun;
        break;
      case 'x':
      case 'X':
        disk_car->arch=&arch_xbox;
        break;
      case 'q':
      case 'Q':
        return 1;
    }
  }
  autoset_unit(disk_car);
  return 0;
}

void screen_buffer_to_interface()
{
  {
    int i;
    int pos=intr_nbr_line-DUMP_MAX_LINES<0?0:intr_nbr_line-DUMP_MAX_LINES;
    if(intr_nbr_line<MAX_LINES && intr_buffer_screen[intr_nbr_line][0]!='\0')
      intr_nbr_line++;
    /* curses interface */
    for (i=pos; i<intr_nbr_line && i<MAX_LINES && (i-pos)<DUMP_MAX_LINES; i++)
    {
      wmove(stdscr,DUMP_Y+1+i-pos,DUMP_X);
      wclrtoeol(stdscr);
      wprintw(stdscr, "%-*s", COLS, intr_buffer_screen[i]);
    }
    wrefresh(stdscr);
  }
}

int vaff_txt(int line, WINDOW *window, const char *_format, va_list ap)
{
  char buffer[1024];
  int i;
  vsnprintf(buffer,sizeof(buffer),_format,ap);
  buffer[sizeof(buffer)-1]='\0';
  for(i=0;buffer[i]!='\0';)
  {
    char buffer2[1024];
    int j,end=i,end2=i;
    for(j=i;buffer[j]!='\0' && (j-i)<COLUMNS;j++)
      if((buffer[j]==' ' || buffer[j]=='\t') && buffer[j+1]!='?' && buffer[j+1]!='[')
      {
        end=j;
        end2=j;
      }
      else if(buffer[j]=='\n')
      {
        end=j;
        end2=j;
        break;
      }
      else if(buffer[j]=='\\' || buffer[j]=='/')
        end2=j;
    if(end2>end && end-i<COLUMNS*3/4)
      end=end2;
    if(end==i)
      end=j-1;
    if(buffer[j]=='\0')
      end=j;
    wmove(window,line,0);
    line++;
    memcpy(buffer2,&buffer[i],end-i+1);
    buffer2[end-i+1]='\0';
    waddstr(window,buffer2);
    for(i=end;buffer[i]==' ' || buffer[i]=='\t' || buffer[i]=='\n'; i++);
  }
  return line;
}

void display_message(const char*msg)
{
  int pipo=0;
  static struct MenuItem menuGeometry[]=
  {
    { 'Q', "Ok", "" },
    { 0, NULL, NULL }
  };
  WINDOW *window=newwin(0,0,0,0);	/* full screen */
  log_info("%s",msg);
  aff_copy(window);
  mvwaddstr(window,5,0,msg);
  wmenuSimple(window,menuGeometry, pipo);
  delwin(window);
  (void) clearok(stdscr, TRUE);
#ifdef HAVE_TOUCHWIN
  touchwin(stdscr);
#endif
}

#else
#include "log.h"
#include "intrfn.h"

void display_message(const char*msg)
{
  log_info("%s",msg);
}
#endif


