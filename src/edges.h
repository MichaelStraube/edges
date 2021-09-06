/*
 *  edges.h
 *
 *  Copyright (C) 2020 Michael Straube <michael.straubej@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef EDGES_H
#define EDGES_H

#include <limits.h>
#include <stdbool.h>
#include <X11/extensions/Xrandr.h>

#define NUM_EDGES 8

enum edges {
	EDGE_TOP_LEFT = 0,
	EDGE_TOP_RIGHT,
	EDGE_BOTTOM_RIGHT,
	EDGE_BOTTOM_LEFT,
	EDGE_LEFT,
	EDGE_TOP,
	EDGE_RIGHT,
	EDGE_BOTTOM,
	EDGE_NONE
};

struct options {
	bool blocking;
	bool useconfig;
	bool verbose;
};

struct commands {
	/* pointers to commands */
	char *commands[NUM_EDGES];
	/* splitted commands for execvp */
	char **execvpargs[NUM_EDGES];
	/* memory for commands from config file */
	char buffers[NUM_EDGES][LINE_MAX];
};

struct monitors {
	int n;
	XRRMonitorInfo *monitors;
};

struct rect {
	int x;
	int y;
	int w;
	int h;
};

#endif /* EDGES_H */
