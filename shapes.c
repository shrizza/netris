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
 * $Id: shapes.c,v 1.16 1999/05/16 06:56:31 mhw Exp $
 */

#include "netris.h"
#include <stdlib.h>

#define ShapeName(name, dir) \
	shape_ ## name ## _ ## dir

#define PreDecl(name, dir) \
	static Shape ShapeName(name, dir)

// The initY and initX here are TGM defaults
// netris default is 0,0; this is properly accounted for in ShapeIterate()
#define StdShape(name, cmdName, mirror, type, realDir, dir, initY, initX, nextDirCCW, nextDirCW) \
	static Shape ShapeName(name, dir) = { \
		&ShapeName(name, nextDirCCW), \
		&ShapeName(name, nextDirCW), \
		initY, initX, mirror, D_ ## realDir, type, cmds_ ## cmdName }

#define LTDecl(name, cmdName, mirror, type) \
	PreDecl(name, down); \
	PreDecl(name, up); \
	PreDecl(name, right); \
	StdShape(name, cmdName, mirror, type, left, left, 0, 0, down, up); \
	StdShape(name, cmdName, mirror, type, up, up, -1, 0, left, right); \
	StdShape(name, cmdName, mirror, type, right, right, 0, 0, up, down); \
	StdShape(name, cmdName, mirror, type, down, down, 0, 0, right, left)

#define JDecl(name, cmdName, mirror, type) \
	PreDecl(name, down); \
	PreDecl(name, up); \
	PreDecl(name, right); \
	StdShape(name, cmdName, mirror, type, left, left, 0, 0, down, up); \
	StdShape(name, cmdName, mirror, type, up, up, 1, 0, left, right); \
	StdShape(name, cmdName, mirror, type, right, right, 0, 0, up, down); \
	StdShape(name, cmdName, mirror, type, down, down, 0, 0, right, left)

#define IDecl(name, cmdName, mirror, type) \
	PreDecl(name, vert); \
	StdShape(name, cmdName, mirror, type, right, horiz, 0, 0, vert, vert); \
	StdShape(name, cmdName, mirror, type, down, vert, 0, 1, horiz, horiz)

#define SDecl(name, cmdName, mirror, type) \
	PreDecl(name, vert); \
	StdShape(name, cmdName, mirror, type, right, horiz, 0, 0, vert, vert); \
	StdShape(name, cmdName, mirror, type, down, vert, 1, -1, horiz, horiz)

#define ZDecl(name, cmdName, mirror, type) \
	PreDecl(name, vert); \
	StdShape(name, cmdName, mirror, type, right, horiz, 0, 0, vert, vert); \
	StdShape(name, cmdName, mirror, type, down, vert, 0, -1, horiz, horiz)

static Cmd cmds_long[] = { C_back, C_plot, C_forw, C_plot, C_forw, C_plot,
	C_forw, C_plot, C_end };
IDecl(long, long, 0, BT_blue);

static Cmd cmds_square[] = { C_plot, C_forw, C_left, C_plot, C_forw, C_left,
	C_plot, C_forw, C_left, C_plot, C_end };
static Shape shape_square = { &shape_square, &shape_square, 0, -1, D_up, 0, BT_magenta,
	cmds_square };

static Cmd cmds_l[] = { C_right, C_back, C_plot, C_forw, C_plot, C_forw,
	C_plot, C_left, C_forw, C_plot, C_end };
LTDecl(l, l, 0, BT_cyan);
JDecl(j, l, 1, BT_yellow);

static Cmd cmds_t[] = { C_plot, C_forw, C_plot, C_back, C_right, C_forw,
	C_plot, C_back, C_back, C_plot, C_end };
LTDecl(t, t, 0, BT_white);

static Cmd cmds_s[] = { C_back, C_plot, C_forw, C_plot, C_left, C_forw,
	C_plot, C_right, C_forw, C_plot, C_end };
SDecl(s, s, 0, BT_green);
ZDecl(z, s, 1, BT_red);

ShapeOption stdOptions[] = {
	{1, &shape_long_horiz},
	{1, &shape_square},
	{1, &shape_l_down},
	{1, &shape_j_down},
	{1, &shape_t_down},
	{1, &shape_s_horiz},
	{1, &shape_z_horiz},
	{0, NULL}};

Shape *netMapping[] = {
	&shape_long_horiz,
	&shape_long_vert,
	&shape_square,
	&shape_l_down,
	&shape_l_right,
	&shape_l_up,
	&shape_l_left,
	&shape_j_down,
	&shape_j_right,
	&shape_j_up,
	&shape_j_left,
	&shape_t_down,
	&shape_t_right,
	&shape_t_up,
	&shape_t_left,
	&shape_s_horiz,
	&shape_s_vert,
	&shape_z_horiz,
	&shape_z_vert,
	NULL};

ExtFunc void MoveInDir(Dir dir, int dist, int *y, int *x)
{
	switch (dir) {
		case D_down:	*y -= dist; break;
		case D_right:	*x += dist; break;
		case D_up:		*y += dist; break;
		case D_left:	*x -= dist; break;
		default:
			assert(0);
	}
}

ExtFunc Dir RotateDir(Dir dir, int delta)
{
	return 3 & (dir + delta);
}

ExtFunc int ShapeIterate(Shape *s, int scr, int y, int x, int falling,
ExtFunc				ShapeDrawFunc func, void *data)
{
	int i, mirror, result;
	Dir dir;
	BlockType type;

	y += game == GT_TGM_1P ? s->mirrored ? -s->initY : s->initY : 0;
	x += game == GT_TGM_1P ? s->mirrored ? -s->initX : s->initX : 0;
	dir = s->initDir;
	type = falling ? -s->type : s->type;
	mirror = s->mirrored ? -1 : 1;
	for (i = 0; s->cmds[i] != C_end; ++i)
		switch (s->cmds[i]) {
			case C_forw:
				MoveInDir(dir, 1, &y, &x);
				break;
			case C_back:
				MoveInDir(dir, -1, &y, &x);
				break;
			case C_left:
				dir = RotateDir(dir, mirror);
				break;
			case C_right:
				dir = RotateDir(dir, -mirror);
				break;
			case C_plot:
				if ((result = func(scr, y, x, type, data)))
					return result;
				break;
			default:
				assert(0);
		}
	return 0;
}

int BagContains(int piece)
{
	int i;
	for (i = BAG_SIZE - 1 ; i > BAG_SIZE - HIST_BAG_SIZE - 1; i--) {
		if (bag[i] == piece) {
			return 1;
		}
	}
	return 0;
}

ExtFunc void UpdateBag(int piece)
{
	int i;
	for (i = 0; i < BAG_SIZE - 1; i++)
		bag[i] = bag[i + 1];
	bag[BAG_SIZE-1] = piece;
}

ExtFunc int Randomizer(void)
{
	int i, piece = 0;
	for (i = 0; i < PIECE_TRIES; i++) {
		piece = Random(0, 7);
		if (BagContains(piece) == 0)
			return piece;
	}
	return piece;
}

ExtFunc Shape *ChooseOption(ShapeOption *options)
{
	UpdateBag(Randomizer());
	UpdatePreview();
	return options[bag[4]].shape;
}

ExtFunc short ShapeToNetNum(Shape *shape)
{
	int num;

	for (num = 0; netMapping[num]; ++num)
		if (netMapping[num] == shape)
			return num;
	assert(0);
	return 0;
}

ExtFunc Shape *NetNumToShape(short num)
{
	assert(num >= 0 && num < sizeof(netMapping) / sizeof(netMapping[0]) - 1);
	return netMapping[num];
}

/*
 * vi: ts=4 ai
 * vim: noai si
 */
