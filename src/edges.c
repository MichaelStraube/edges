/*
 *  edges.c
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

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <stdarg.h>

#include "edges.h"
#include "util.h"

static const char *myname = "edges", *myver = "2.0.2";

static Display *dpy;
static volatile sig_atomic_t quit;
static struct options options;
static struct monitors monitors;
static struct commands commands;

static void usage(void)
{
	printf("usage: %s [options]...\n\n"
		"options:\n"
		"  --top-left CMD            set top left corner command CMD\n"
		"  --top-right CMD           set top right corner command CMD\n"
		"  --bottom-right CMD        set bottom right corner command CMD\n"
		"  --bottom-left CMD         set bottom left corner command CMD\n\n"
		"  --left CMD                set left edge command CMD\n"
		"  --top CMD                 set top edge command CMD\n"
		"  --right CMD               set right edge command CMD\n"
		"  --bottom CMD              set bottom edge command CMD\n\n"
		"  -b, --no-blocking         do not wait until child process exits\n"
		"  -c, --use-config          use config file, ignore passed commands\n\n"
		"  -h, --help                show this help message and exit\n"
		"  -v, --verbose             print pointer position and command calls\n"
		"  --version                 display the version and exit\n\n",
		myname);
}

static void version(void)
{
	printf("%s %s\n", myname, myver);
}

static void fatal(const char *format, ...)
{
	char buffer[LINE_MAX];
	va_list args;

	va_start(args, format);
	vsprintf(buffer, format, args);
	if (errno) {
		fprintf(stderr, "%s: %s\n", buffer, strerror(errno));
	} else {
		fprintf(stderr, "%s\n", buffer);
	}
	va_end(args);
}

static void (*warn)(const char *format, ...) = &fatal;

static void cleanup(void)
{
	int i;

	for (i = 0; i < NUM_EDGES; i++) {
		wordsplit_free(commands.execvpargs[i]);
	}
	if (monitors.monitors) {
		XRRFreeMonitors(monitors.monitors);
	}
	if (dpy) {
		XCloseDisplay(dpy);
	}
}

static void sighandler(int signal __attribute__((unused)))
{
	quit = 1;
}

static void parse_options(int argc, char *argv[])
{
	enum {
		OPT_TOP_LEFT = 1,
		OPT_TOP_RIGHT,
		OPT_BOTTOM_RIGHT,
		OPT_BOTTOM_LEFT,
		OPT_LEFT,
		OPT_TOP,
		OPT_RIGHT,
		OPT_BOTTOM,
		OPT_VERSION
	};
	static const struct option opts[] = {
		{"top-left",     required_argument, 0, OPT_TOP_LEFT},
		{"top-right",    required_argument, 0, OPT_TOP_RIGHT},
		{"bottom-right", required_argument, 0, OPT_BOTTOM_RIGHT},
		{"bottom-left",  required_argument, 0, OPT_BOTTOM_LEFT},
		{"left",         required_argument, 0, OPT_LEFT},
		{"top",          required_argument, 0, OPT_TOP},
		{"right",        required_argument, 0, OPT_RIGHT},
		{"bottom",       required_argument, 0, OPT_BOTTOM},
		{"no-blocking",  no_argument,       0, 'b'},
		{"use-config",   no_argument,       0, 'c'},
		{"help",         no_argument,       0, 'h'},
		{"verbose",      no_argument,       0, 'v'},
		{"version",      no_argument,       0, OPT_VERSION},
		{ 0, 0, 0, 0}
	};

	while (true) {
		int c = getopt_long(argc, argv, "bchv", opts, NULL);

		if (c == -1) {
			break;
		}

		switch (c) {
		case OPT_TOP_LEFT:
			commands.commands[EDGE_TOP_LEFT] = optarg;
			break;
		case OPT_TOP_RIGHT:
			commands.commands[EDGE_TOP_RIGHT] = optarg;
			break;
		case OPT_BOTTOM_RIGHT:
			commands.commands[EDGE_BOTTOM_RIGHT] = optarg;
			break;
		case OPT_BOTTOM_LEFT:
			commands.commands[EDGE_BOTTOM_LEFT] = optarg;
			break;
		case OPT_LEFT:
			commands.commands[EDGE_LEFT] = optarg;
			break;
		case OPT_TOP:
			commands.commands[EDGE_TOP] = optarg;
			break;
		case OPT_RIGHT:
			commands.commands[EDGE_RIGHT] = optarg;
			break;
		case OPT_BOTTOM:
			commands.commands[EDGE_BOTTOM] = optarg;
			break;
		case 'b':
			options.blocking = false;
			break;
		case 'c':
			options.useconfig = true;
			break;
		case 'v':
			options.verbose = true;
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case OPT_VERSION:
			version();
			exit(EXIT_SUCCESS);
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	if (optind < 2) {
		fatal("%s: no options", argv[0]);
		usage();
		exit(EXIT_FAILURE);
	}
}

static void run(char **args)
{
	pid_t pid;

	if (!args) {
		if (options.verbose) {
			printf("Command: None\n");
		}
		return;
	}

	if (options.verbose) {
		int i = 0;

		/* print the splitted substrings */
		printf("Command: ");
		while (args[i]) {
			printf("%s ", args[i++]);
		}
		printf("\n");
	}

	pid = fork();

	if (pid == -1) {
		warn("fork failed");
		return;
	}

	if (pid == 0) {
		/* child */
		execvp(args[0], args);

		/* if execvp returns it must have failed */
		warn("execvp failed");
		_exit(0);
	} else {
		/* parent */
		if (options.blocking) {
			wait(NULL);
		}
	}
}

static void init_signals(void)
{
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_handler = &sighandler;
	act.sa_flags = 0;

	if (sigaction(SIGINT, &act, NULL) == -1 ||
	    sigaction(SIGTERM, &act, NULL) == -1 ||
	    sigaction(SIGHUP, &act, NULL) == -1) {
		fatal("Failed to set up signal handling");
		exit(EXIT_FAILURE);
	}
}

static void init_x11(int *xi_op)
{
	int ev, err, major, minor;
	XIEventMask mask;

	if (getenv("WAYLAND_DISPLAY")) {
		fatal("Global pointer query not supported on Wayland");
		exit(EXIT_FAILURE);
	}

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fatal("XOpenDisplay() failed");
		exit(EXIT_FAILURE);
	}

	if (!XQueryExtension(dpy, "XInputExtension", xi_op, &ev, &err)) {
		fatal("XInput extension not available");
		exit(EXIT_FAILURE);
	}

	if (!XRRQueryExtension(dpy, &ev, &err) ||
	    !XRRQueryVersion(dpy, &major, &minor)) {
		fatal("Xrandr extension not available");
		exit(EXIT_FAILURE);
	}
	if (!(major > 1 || (major == 1 && minor >= 5))) {
		fatal("Xrandr version < 1.5");
		exit(EXIT_FAILURE);
	}

	/* select raw motion events */
	mask.deviceid = XIAllMasterDevices;
	mask.mask_len = XIMaskLen(XI_RawMotion);
	mask.mask = calloc(mask.mask_len, sizeof(*mask.mask));
	if (!mask.mask) {
		fatal("Failed to allocate memory");
		exit(EXIT_FAILURE);
	}
	XISetMask(mask.mask, XI_RawMotion);
	XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);
	free(mask.mask);
}

static void read_config(void)
{
	const char *home;
	char file[PATH_MAX];
	FILE *fp;
	char line[LINE_MAX];
	int linenum = 0;

	home = getenv("HOME");
	if (!home) {
		fatal("Failed to get home directory");
		exit(EXIT_FAILURE);
	}
	snprintf(file, PATH_MAX, "%s/.config/edges/edges.rc", home);

	fp = fopen(file, "r");
	if (!fp) {
		fatal("Failed to open '%s'", file);
		exit(EXIT_FAILURE);
	}

	while (safe_fgets(line, LINE_MAX, fp)) {
		char *key, *value;
		size_t line_len;
		int i;

		linenum++;

		line_len = strtrim(line);

		if (line_len == 0 || line[0] == '#') {
			continue;
		}

		/* strsep modifies the 'line' string: 'key \0 value' */
		key = line;
		value = line;
		strsep(&value, "=");
		if (!value) {
			fclose(fp);
			fatal("%s:%d - syntax error", file, linenum);
			exit(EXIT_FAILURE);
		}
		strtrim(key);
		strtrim(value);

		for (i = 0; i < NUM_EDGES; i++) {
			const char *needles[NUM_EDGES] = {
				"top-left",
				"top-right",
				"bottom-right",
				"bottom-left",
				"left",
				"top",
				"right",
				"bottom"
			};

			if (strcmp(key, needles[i]) == 0) {
				int n = snprintf(commands.buffers[i], LINE_MAX, "%s", value);

				if (n < 0 || n >= LINE_MAX) {
					fclose(fp);
					fatal("%s: string too long", __func__);
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	fclose(fp);
}

static char **split(char *str)
{
	char **ret;

	if (!str || *str == '\0') {
		return NULL;
	}

	ret = wordsplit(str);
	if (!ret) {
		fatal("wordsplit failed");
		exit(EXIT_FAILURE);
	}

	if (strtrim(*ret) == 0) {
		wordsplit_free(ret);
		return NULL;
	}

	return ret;
}

static void prepare_commands(void)
{
	int i;

	/* prepare commands for execvp */
	for (i = 0; i < NUM_EDGES; i++) {
		char **splitted;

		if (options.useconfig) {
			read_config();
			splitted = split(commands.buffers[i]);
		} else {
			splitted = split(commands.commands[i]);
		}

		commands.execvpargs[i] = splitted;
	}
}

static void get_monitors(void)
{
	int n;
	XRRMonitorInfo *m;

	m = XRRGetMonitors(dpy, DefaultRootWindow(dpy), True, &n);
	if (!m) {
		fatal("Failed to get monitors");
		exit(EXIT_FAILURE);
	}
	monitors.n = n;
	monitors.monitors = m;
}

static bool point_in_rect(int x, int y, struct rect *rect)
{
	if ((rect->x <= x && x < rect->x + rect->w) &&
	    (rect->y <= y && y < rect->y + rect->h)) {
		return true;
	}
	return false;
}

static int pointer_in_monitor(int x, int y)
{
	int i;

	for (i = 0; i < monitors.n; i++) {
		struct rect rect;

		rect.x = monitors.monitors[i].x;
		rect.y = monitors.monitors[i].y;
		rect.w = monitors.monitors[i].width;
		rect.h = monitors.monitors[i].height;

		if (point_in_rect(x, y, &rect)) {
			return i;
		}
	}

	return -1;
}

static void get_max_xy(int x, int y, int *max_x, int *max_y)
{
	int m, w, h, off;

	*max_x = DisplayWidth(dpy, DefaultScreen(dpy)) - 1;
	*max_y = DisplayHeight(dpy, DefaultScreen(dpy)) - 1;

	if (monitors.n == 1) {
		return;
	}

	m = pointer_in_monitor(x, y);
	if (m < 0) {
		fatal("pointer_in_monitor failed");
		exit(EXIT_FAILURE);
	}

	w = monitors.monitors[m].width;
	off = monitors.monitors[m].x;

	if (off + w <= *max_x) {
		*max_x = off + w - 1;
	}

	h = monitors.monitors[m].height;
	off = monitors.monitors[m].y;

	if (off + h <= *max_y) {
		*max_y = off + h - 1;
	}
}

static enum edges in_edge(int x, int y, int max_x, int max_y, int offset)
{
	if (x == 0 && y == 0) {
		return EDGE_TOP_LEFT;
	} else if (x == max_x && y == 0) {
		return EDGE_TOP_RIGHT;
	} else if (x == max_x && y == max_y) {
		return EDGE_BOTTOM_RIGHT;
	} else if (x == 0 && y == max_y) {
		return EDGE_BOTTOM_LEFT;
	} else if (x == 0 && y > offset && y < max_y - offset) {
		return EDGE_LEFT;
	} else if (y == 0 && x > offset && x < max_x - offset) {
		return EDGE_TOP;
	} else if (x == max_x && y > offset && y < max_y - offset) {
		return EDGE_RIGHT;
	} else if (y == max_y && x > offset && x < max_x - offset) {
		return EDGE_BOTTOM;
	} else {
		return EDGE_NONE;
	}
}

static void main_loop(int xi_op)
{
	XEvent event;
	Window root_ret, child_ret;
	int x, y, win_x_ret, win_y_ret;
	unsigned int mask_ret;
	struct pollfd fds;
	int old_x = 1, old_y = 1;
	int max_x, max_y, offset;
	enum edges edge;

	fds.fd = ConnectionNumber(dpy);
	fds.events = POLLIN;
	fds.revents = 0;

	do {
		if (!XPending(dpy)) {
			continue;
		}

		XNextEvent(dpy, &event);
		XGetEventData(dpy, &event.xcookie);

		/* was pointer moved? */
		if (event.xcookie.type == GenericEvent &&
		    event.xcookie.extension == xi_op &&
		    event.xcookie.evtype == XI_RawMotion) {

			XQueryPointer(dpy, DefaultRootWindow(dpy),
				      &root_ret, &child_ret, &x, &y,
				      &win_x_ret, &win_y_ret, &mask_ret);

			/* now we have the position in x, y */

			if (options.verbose) {
				printf("%d  %d\n", x, y);
			}

			get_max_xy(x, y, &max_x, &max_y);
			offset = max_y * 0.25;

			/* ensure we run commands only once on edge hits */
			if ((x == old_x && y == old_y) ||
			    (x == old_x && y > offset && y < max_y - offset) ||
			    (y == old_y && x > offset && x < max_x - offset)) {
				XFreeEventData(dpy, &event.xcookie);
				continue;
			}

			edge = in_edge(x, y, max_x, max_y, offset);
			if (edge != EDGE_NONE) {
				run(commands.execvpargs[edge]);
			}

			old_x = x;
			old_y = y;
		}
		XFreeEventData(dpy, &event.xcookie);
	} while (!quit && poll(&fds, 1, -1) > -1);
}

int main(int argc, char *argv[])
{
	int xi_op;

	if (atexit(cleanup)) {
		fatal("Failed to register exit handler");
		return EXIT_FAILURE;
	}

	options.blocking = true;
	options.useconfig = false;
	options.verbose = false;

	parse_options(argc, argv);
	prepare_commands();

	init_signals();
	init_x11(&xi_op);

	get_monitors();
	main_loop(xi_op);

	return EXIT_SUCCESS;
}
