/*
 * Netris -- A free networked version of T*tris
 * Copyright (C) 1994-1996,1999  Mark H. Weaver <mhw@netris.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: curses.c,v 1.33 1999/05/16 06:56:25 mhw Exp $
 */

#include "netris.h"
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <term.h>
#include <curses.h>
#include <string.h>
#include <stdlib.h>

#ifdef NCURSES_VERSION
# define HAVE_NCURSES
#endif

#ifdef HAVE_NCURSES
static struct
{
	BlockType type;
	short color;
} myColorTable[] =
{
	{ BT_white,		COLOR_WHITE },
	{ BT_blue,		COLOR_BLUE },
	{ BT_magenta,	COLOR_MAGENTA },
	{ BT_cyan,		COLOR_CYAN },
	{ BT_yellow,	COLOR_YELLOW },
	{ BT_green,		COLOR_GREEN },
	{ BT_red,		COLOR_RED },
	{ BT_none, 0 }
};
#endif

static void PlotBlock1(int scr, int y, int x, BlockType type);
static MyEventType KeyGenFunc(EventGenRec *gen, MyEvent *event);

static EventGenRec keyGen =
		{ NULL, 0, FT_read, STDIN_FILENO, KeyGenFunc, EM_key };

static int boardYPos[MAX_SCREENS], boardXPos[MAX_SCREENS];
static int statusYPos, statusXPos;
static int haveColor;
static int screens_dirty = 0;

static char *term_vi;	/* String to make cursor invisible */
static char *term_ve;	/* String to make cursor visible */

ExtFunc void InitScreens(void)
{
	MySigSet oldMask;

	GetTermcapInfo();

	/*
	 * Block signals while initializing curses.  Otherwise a badly timed
	 * Ctrl-C during initialization might leave the terminal in a bad state.
	 */
	BlockSignals(&oldMask, SIGINT, 0);
	initscr();

#ifdef CURSES_HACK
	{
		extern char *CS;

		CS = 0;
	}
#endif

#ifdef HAVE_NCURSES
	haveColor = colorEnable && has_colors();
	if (haveColor)
	{
		int i = 0;

		start_color();
		for (i = 0; myColorTable[i].type != BT_none; ++i)
			init_pair(myColorTable[i].type, COLOR_BLACK,
					myColorTable[i].color);
	}
#else
	haveColor = 0;
#endif

	AtExit(CleanupScreens);
	screens_dirty = 1;
	RestoreSignals(NULL, &oldMask);

	cbreak();
	noecho();
	OutputTermStr(term_vi, 0);
	AddEventGen(&keyGen);

	move(0, 0);
	addstr("Netris ");
	addstr(version_string);
	addstr(" (C) 1994-1996,1999  Mark H. Weaver     "
			"\"netris -h\" for more info");
	statusYPos = 22;
	statusXPos = 0;
}

ExtFunc void CleanupScreens(void)
{
	if (screens_dirty) {
		RemoveEventGen(&keyGen);
		endwin();
		OutputTermStr(term_ve, 1);
		screens_dirty = 0;
	}
}

ExtFunc void GetTermcapInfo(void)
{
	char *term, *buf, *data;
	int bufSize = 10240;

	if (!(term = getenv("TERM")))
		return;
	if (tgetent(scratch, term) == 1) {
		/*
		 * Make the buffer HUGE, since tgetstr is unsafe.
		 * Allocate it on the heap too.
		 */
		data = buf = malloc(bufSize);

		/*
		 * There is no standard include file for tgetstr, no prototype
		 * definitions.  I like casting better than using my own prototypes
		 * because if I guess the prototype, I might be wrong, especially
		 * with regards to "const".
		 */
		term_vi = (char *)tgetstr("vi", &data);
		term_ve = (char *)tgetstr("ve", &data);

		/* Okay, so I'm paranoid; I just don't like unsafe routines */
		if (data > buf + bufSize)
			fatal("tgetstr overflow, you must have a very sick termcap");

		/* Trim off the unused portion of buffer */
		buf = realloc(buf, data - buf);
	}

	/*
	 * If that fails, use hardcoded vt220 codes.
	 * They don't seem to do anything bad on vt100's, so
	 * we'll try them just in case they work.
	 */
	if (!term_vi || !term_ve) {
		static char *vts[] = {
				"vt100", "vt101", "vt102",
				"vt200", "vt220", "vt300",
				"vt320", "vt400", "vt420",
				"screen", "xterm", NULL };
		int i;

		for (i = 0; vts[i]; i++)
			if (!strcmp(term, vts[i]))
			{
				term_vi = "\033[?25l";
				term_ve = "\033[?25h";
				break;
			}
	}
	if (!term_vi || !term_ve)
		term_vi = term_ve = NULL;
}

ExtFunc void OutputTermStr(char *str, int flush)
{
	if (str) {
		fputs(str, stdout);
		if (flush)
			fflush(stdout);
	}
}

ExtFunc void InitScreen(int scr, int border)
{
	int y, x;

	if (scr == 0) {
		boardXPos[scr] = 1;
		boardYPos[scr] = 22;
	} else if (scr == 2) {
		boardXPos[scr] = 23;
		boardYPos[scr] = 4;
	} else {
		boardXPos[scr] = 59;
		boardYPos[scr] = 22;
	}
	statusXPos = 23;
	statusYPos = 22;
	if (border == 1) {
		for (y = boardVisible[scr] - 1; y >= 0; --y) {
			move(boardYPos[scr] - y, boardXPos[scr] - 1);
			addch('|');
			for (x = boardWidth[scr] - 1; x >= 0; --x)
				addstr("  ");
			move(boardYPos[scr] - y, boardXPos[scr] + 2 * boardWidth[scr]);
			addch('|');
		}
		for (y = boardVisible[scr]; y >= -1; y -= boardVisible[scr] + 1) {
			move(boardYPos[scr] - y, boardXPos[scr] - 1);
			addch('+');
			for (x = boardWidth[scr] - 1; x >= 0; --x)
				addstr("--");
			addch('+');
		}
	}
}

ExtFunc void CleanupScreen(int scr)
{
}

static void PlotBlock1(int scr, int y, int x, BlockType type)
{
	int colorIndex = abs(type);

	move(boardYPos[scr] - y, boardXPos[scr] + 2 * x);

	if (type == BT_none)
		addstr("  ");
	else
	{
		if (standoutEnable)
		{
#ifdef HAVE_NCURSES
			if (haveColor)
				attrset(COLOR_PAIR(colorIndex));
			else
#endif
				standout();
		}

		addstr(type > 0 ? "[]" : "$$");
		standend();
	}
}

ExtFunc void PlotBlock(int scr, int y, int x, BlockType type)
{
	if (y >= 0 && y < boardVisible[scr] && x >= 0 && x < boardWidth[scr])
		PlotBlock1(scr, y, x, type);
}

ExtFunc void PlotUnderline(int scr, int x, int flag)
{
	move(boardYPos[scr] + 1, boardXPos[scr] + 2 * x);
	addstr(flag ? "==" : "--");
}

ExtFunc void ShowDisplayInfo(void)
{
	UpdateEnemyLines();
	UpdateMyLinesCleared();
	UpdateWinLoss();
	UpdateGameState();
	UpdateSeed();
	UpdateSpeed();
	UpdateLevel();
	UpdateRobot();
}

ExtFunc void UpdateEnemyLines(void)
{
	if (game == GT_classicTwo) {
		move(statusYPos - 5, statusXPos);
		printw("Enemy lines: %3d/%4d", enemyLinesCleared, enemyTotalLinesCleared);
	}
}

ExtFunc void UpdateGameState(void)
{
	move(statusYPos - 1, statusXPos);
	switch(gameState) {
	case STATE_WAIT_CONNECTION:
		addstr("Waiting for opponent...      ");
		break;
	case STATE_WAIT_KEYPRESS:
		addstr("Press the key for a new game.");
		break;
	default:
		addstr("                             ");
	}
}

ExtFunc void UpdateRobot(void)
{
	if (robotEnable) {
		move(statusYPos - 7, statusXPos);
		if (fairRobot)
			addstr("Controlled by a fair robot");
		else
			addstr("Controlled by a robot     ");
	}
	if (opponentFlags & SCF_usingRobot) {
		move(statusYPos - 6, statusXPos);
		if (opponentFlags & SCF_fairRobot)
			addstr("The opponent is a fair robot");
		else
			addstr("The opponent is a robot     ");
	}
}

ExtFunc void UpdateMyLinesCleared(void)
{
	move(statusYPos - 4, statusXPos);
	printw("My lines:    %3d/%4d", myLinesCleared, myTotalLinesCleared);
}

ExtFunc void UpdateSeed(void)
{
	move(statusYPos - 12, statusXPos);
	printw("Seed: %d", initSeed);
}

ExtFunc void UpdateSpeed(void)
{
	if (game == GT_TGM_1P)
		return;
	move(statusYPos - 11, statusXPos);
	printw("Speed: %dms", speed / 1000);
}

ExtFunc void UpdatePreview(void)
{
	PlotShape(stdOptions[bag[5]].shape, 2, 1, 1, 0);
	PlotShape(stdOptions[bag[6]].shape, 2, 1, 6, 0);
	PlotShape(stdOptions[bag[7]].shape, 2, 1, 11, 0);
	RefreshBoard(2);
	EraseShape(stdOptions[bag[5]].shape, 2, 1, 1);
	EraseShape(stdOptions[bag[6]].shape, 2, 1, 6);
	EraseShape(stdOptions[bag[7]].shape, 2, 1, 11);
}

ExtFunc void UpdateWinLoss(void)
{
	if (game != GT_classicTwo)
		return;
	move(statusYPos - 3, statusXPos);
	printw("Won:  %3d", won);
	move(statusYPos - 2, statusXPos);
	printw("Lost: %3d", lost);
}

ExtFunc void UpdateLevel(void)
{
	if (game != GT_TGM_1P)
		return;
	move(statusYPos - 9, statusXPos);
	printw("Level:    %d / %d", level, sectionClear);
}

ExtFunc void UpdateMedals(void)
{
	if (game != GT_TGM_1P)
		return;
	UpdateMedalSK();
	UpdateMedalAC();
	UpdateMedalRE();
	standend();
}

ExtFunc void UpdateMedalSK(void)
{
	if (counterClears[3] < 10)
		return;
	move(statusYPos - 8, statusXPos);
	if (counterClears[3] >= 35)			attrset(COLOR_PAIR(BT_yellow));
	else if (counterClears[3] >= 20)	attrset(COLOR_PAIR(BT_white));
	else 								attrset(COLOR_PAIR(BT_red));
	printw(" SK ");
}

ExtFunc void UpdateMedalAC(void)
{
	if (counterBravo < 1)
		return;
	move(statusYPos - 8, statusXPos + 5);
	if (counterBravo >= 3)		attrset(COLOR_PAIR(BT_yellow));
	else if (counterBravo >= 2)	attrset(COLOR_PAIR(BT_white));
	else 						attrset(COLOR_PAIR(BT_red));
	printw(" AC ");
}

ExtFunc void UpdateMedalRE(void)
{
	if (counterRecovery < 1)
		return;
	move(statusYPos - 8, statusXPos + 10);
	if (counterRecovery >= 3)		attrset(COLOR_PAIR(BT_yellow));
	else if (counterRecovery >= 2)	attrset(COLOR_PAIR(BT_white));
	else 							attrset(COLOR_PAIR(BT_red));
	printw(" RE ");
}

ExtFunc void UpdateMedalCO(void)
{
	if (bestCombo < 4)
		return;
	move(statusYPos - 8, statusXPos + 15);
	if (bestCombo >= 7)			attrset(COLOR_PAIR(BT_yellow));
	else if (bestCombo >= 5)	attrset(COLOR_PAIR(BT_white));
	else 						attrset(COLOR_PAIR(BT_red));
	printw(" CO ");
}

ExtFunc void UpdateOpponentDisplay(void)
{
	move(1, 0);
	printw("Playing %s@%s", opponentName, opponentHost);
}

ExtFunc void ShowPause(int pausedByMe, int pausedByThem)
{
	move(statusYPos - 3, statusXPos);
	if (pausedByThem)
		addstr("Game paused by opponent");
	else
		addstr("                       ");
	move(statusYPos - 2, statusXPos);
	if (pausedByMe)
		addstr("Game paused by you     ");
	else
		addstr("                       ");
}

ExtFunc void Message(char *s)
{
	static int line = 0;

	move(statusYPos - 20 + line, statusXPos);
	addstr(s);	/* XXX Should truncate long lines */
	clrtoeol();
	line = (line + 1) % 10;
	move(statusYPos - 20 + line, statusXPos);
	clrtoeol();
}

ExtFunc void RefreshScreen(void)
{
	static char timeStr[2][32];
	time_t theTime;

	time(&theTime);
	strftime(timeStr[0], 30, "%I:%M %p", localtime(&theTime));
	/* Just in case the local curses library sucks */
	if (strcmp(timeStr[0], timeStr[1]))
	{
		move(statusYPos, statusXPos);
		addstr(timeStr[0]);
		strcpy(timeStr[1], timeStr[0]);
	}
	move(boardYPos[0] + 1, boardXPos[0] + 2 * boardWidth[0] + 1);
	refresh();
}

ExtFunc void ScheduleFullRedraw(void)
{
	touchwin(stdscr);
}

static MyEventType KeyGenFunc(EventGenRec *gen, MyEvent *event)
{
	if (MyRead(gen->fd, &event->u.key, 1))
		return E_key;
	else
		return E_none;
}

/*
 * vi: ts=4 ai
 * vim: noai si
 */
