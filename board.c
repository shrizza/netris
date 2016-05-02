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
 * $Id: board.c,v 1.15 1999/05/16 06:56:24 mhw Exp $
 */

#include "netris.h"
#include <stdlib.h>

#ifdef DEBUG_FALLING
# define B_OLD
#else
# define B_OLD abs
#endif

static BlockType board[MAX_SCREENS][MAX_BOARD_HEIGHT][MAX_BOARD_WIDTH];
static BlockType oldBoard[MAX_SCREENS][MAX_BOARD_HEIGHT][MAX_BOARD_WIDTH];
static unsigned int changed[MAX_SCREENS][MAX_BOARD_HEIGHT];
static int falling[MAX_SCREENS][MAX_BOARD_WIDTH];
static int oldFalling[MAX_SCREENS][MAX_BOARD_WIDTH];

ExtFunc void InitBoard(int scr)
{
	int s,w,h;

	for(s = 0 ; s < MAX_SCREENS ; s++)
		for(h = 0 ; h < MAX_BOARD_HEIGHT ; h++)
			for(w = 0 ; w < MAX_BOARD_WIDTH ; w++) {
				board[s][h][w] = 0;
				oldBoard[s][h][w] = 0;
				changed[s][h] = 0;
				falling[s][w] = 0;
				oldFalling[s][w] = 0;
			}

	boardHeight[scr] = MAX_BOARD_HEIGHT;
	boardVisible[scr] = 20;
	boardWidth[scr] = 10;
	InitScreen(scr, 1);
}

ExtFunc void InitPreview(int scr)
{
	int w,h;

	for(h = 0 ; h < 4 ; h++)
		for(w = 0 ; w < 14 ; w++) {
			board[2][h][w] = 0;
			oldBoard[2][h][w] = 0;
			changed[2][h] = 0;
			falling[2][w] = 0;
			oldFalling[2][w] = 0;
		}

	boardHeight[scr] = 4;
	boardVisible[scr] = 4;
	boardWidth[scr] = 14;
	InitScreen(scr, 0);
}

ExtFunc void CleanupBoard(int scr)
{
	CleanupScreen(scr);
}

ExtFunc BlockType GetBlock(int scr, int y, int x)
{
	if (y < 0 || x < 0 || x >= boardWidth[scr])
		return BT_wall;
	else if (y >= boardHeight[scr])
		return BT_none;
	else
		return abs(board[scr][y][x]);
}

ExtFunc void SetBlock(int scr, int y, int x, BlockType type)
{
	if (y >= 0 && y < boardHeight[scr] && x >= 0 && x < boardWidth[scr]) {
		if (y < boardVisible[scr])
			falling[scr][x] += (type < 0) - (board[scr][y][x] < 0);
		board[scr][y][x] = type;
		changed[scr][y] |= 1 << x;
	}
}

ExtFunc int RefreshBoard(int scr)
{
	int y, x, any = 0;
	unsigned int c;
	BlockType b;

	for (y = boardVisible[scr] - 1; y >= 0; --y)
		if ((c = changed[scr][y])) {
			if (robotEnable) {
				RobotCmd(0, "RowUpdate %d %d", scr, y);
				for (x = 0; x < boardWidth[scr]; ++x) {
					b = board[scr][y][x];
					if (fairRobot)
						b = abs(b);
					RobotCmd(0, " %d", b);
				}
				RobotCmd(0, "\n");
			}
			changed[scr][y] = 0;
			any = 1;
			for (x = 0; c; (c >>= 1), (++x))
				if ((c & 1) && B_OLD(board[scr][y][x])!=oldBoard[scr][y][x]) {
					PlotBlock(scr, y, x, B_OLD(board[scr][y][x]));
					oldBoard[scr][y][x] = B_OLD(board[scr][y][x]);
				}
		}
	if (robotEnable)
		RobotTimeStamp();
	for (x = 0; x < boardWidth[scr]; ++x)
		if (oldFalling[scr][x] != !!falling[scr][x]) {
			oldFalling[scr][x] = !!falling[scr][x];
			PlotUnderline(scr, x, oldFalling[scr][x]);
			any = 1;
		}
	return any;
}

ExtFunc int PlotFunc(int scr, int y, int x, BlockType type, void *data)
{
	SetBlock(scr, y, x, type);
	return 0;
}

ExtFunc int EraseFunc(int scr, int y, int x, BlockType type, void *data)
{
	SetBlock(scr, y, x, BT_none);
	return 0;
}

ExtFunc int CollisionFunc(int scr, int y, int x, BlockType type, void *data)
{
	return GetBlock(scr, y, x) != BT_none;
}

ExtFunc int VisibleFunc(int scr, int y, int x, BlockType type, void *data)
{
	return (y >= 0 && y < boardVisible[scr] && x >= 0 && x < boardWidth[scr]);
}

ExtFunc void PlotShape(Shape *shape, int scr, int y, int x, int falling)
{
	ShapeIterate(shape, scr, y, x, falling, PlotFunc, NULL);
}

ExtFunc void EraseShape(Shape *shape, int scr, int y, int x)
{
	ShapeIterate(shape, scr, y, x, 0, EraseFunc, NULL);
}

ExtFunc int ShapeFits(Shape *shape, int scr, int y, int x)
{
	return !ShapeIterate(shape, scr, y, x, 0, CollisionFunc, NULL);
}

ExtFunc int ShapeVisible(Shape *shape, int scr, int y, int x)
{
	return ShapeIterate(shape, scr, y, x, 0, VisibleFunc, NULL);
}

ExtFunc int MovePiece(int scr, int deltaY, int deltaX)
{
	int result;

	EraseShape(curShape[scr], scr, curY[scr], curX[scr]);
	result = ShapeFits(curShape[scr], scr, curY[scr] + deltaY,
				curX[scr] + deltaX);
	if (result) {
		curY[scr] += deltaY;
		curX[scr] += deltaX;
	}
	PlotShape(curShape[scr], scr, curY[scr], curX[scr], 1);
	return result;
}

ExtFunc int RotatePiece(int scr, int ccw)
{
	int result, result_l, result_r;

	EraseShape(curShape[scr], scr, curY[scr], curX[scr]);
	result = ShapeFits(ccw ? curShape[scr]->rotateToCCW : curShape[scr]->rotateToCW, scr, curY[scr], curX[scr]);
	if (game == GT_TGM_1P) {
			result_l = ShapeFits(ccw ? curShape[scr]->rotateToCCW : curShape[scr]->rotateToCW, scr, curY[scr], curX[scr] - 1);
			result_r = ShapeFits(ccw ? curShape[scr]->rotateToCCW : curShape[scr]->rotateToCW, scr, curY[scr], curX[scr] + 1);
			if (result || result_l || result_r)
				curShape[scr] = ccw ? curShape[scr]->rotateToCCW : curShape[scr]->rotateToCW;
			if (!result && result_l)
				curX[scr]--;	// wallkick right
			else if (!result && !result_l && result_r)
				curX[scr]++;	// wallkick left
	} else {
			if (result)
				curShape[scr] = ccw ? curShape[scr]->rotateToCCW : curShape[scr]->rotateToCW;
	}
	PlotShape(curShape[scr], scr, curY[scr], curX[scr], 1);
	return result;
}

ExtFunc int DropPiece(int scr)
{
	int count = 0;

	EraseShape(curShape[scr], scr, curY[scr], curX[scr]);
	while (ShapeFits(curShape[scr], scr, curY[scr] - 1, curX[scr])) {
		--curY[scr];
		++count;
	}
	PlotShape(curShape[scr], scr, curY[scr], curX[scr], 1);
	return count;
}

ExtFunc int LineIsFull(int scr, int y)
{
	int x;

	for (x = 0; x < boardWidth[scr]; ++x)
		if (GetBlock(scr, y, x) == BT_none)
			return 0;
	return 1;
}

ExtFunc int CheckBravo(int scr)
{
	int x;
	for (x = 0; x < boardWidth[scr]; ++x)
		if (GetBlock(scr, 0, x) != BT_none)
			return 0;
	return 1;
}

ExtFunc int CountBlocks(int scr)
{
	int x,y,c;
	for (x = 0; x < boardWidth[scr]; ++x)
		for (y = 0; y < boardHeight[scr]; ++y)
			if (GetBlock(scr, y, x) != BT_none)
				c++;
	return c;
}

ExtFunc void CopyLine(int scr, int from, int to)
{
	int x;

	if (from != to)
		for (x = 0; x < boardWidth[scr]; ++x)
			SetBlock(scr, to, x, GetBlock(scr, from, x));
}

ExtFunc int ClearFullLines(int scr)
{
	int from, to;

	from = to = 0;
	while (to < boardHeight[scr]) {
		while (LineIsFull(scr, from))
			++from;
		CopyLine(scr, from++, to++);
	}
	return from - to;
}

ExtFunc void FreezePiece(int scr)
{
	int y, x;
	BlockType type;

	for (y = 0; y < boardHeight[scr]; ++y)
		for (x = 0; x < boardWidth[scr]; ++x)
			if ((type = board[scr][y][x]) < 0)
				SetBlock(scr, y, x, -type);
}

ExtFunc void InsertJunk(int scr, int count, int column)
{
	int y, x;

	for (y = boardHeight[scr] - count - 1; y >= 0; --y)
		CopyLine(scr, y, y + count);
	for (y = 0; y < count; ++y)
		for (x = 0; x < boardWidth[scr]; ++x)
			SetBlock(scr, y, x, (x == column) ? BT_none : BT_white);
	curY[scr] += count;
}

/*
 * vi: ts=4 ai
 * vim: noai si
 */
