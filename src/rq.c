/**
 * MIT/X Consortium License
 * 
 * Copyright © 2015  Mattias Andrée <maandree@member.fsf.org>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#define t(...) do { if (__VA_ARGS__) goto fail; } while (0)



/**
 * The default word rate.
 */
#define DEFAULT_RATE  120  /* 2 hz */



/**
 * The name of the process.
 */
static const char *argv0;



/**
 * Get the selected word rate by reading
 * the environment variable RQ_RATE.
 * 
 * @return  The rate in words per minute.
 */
static long get_word_rate(void)
{
	char *s;
	char *e;
	long r;

	errno = 0;
	s = getenv("RQ_RATE");
	if (!s || !*s || !isdigit(*s))
		return DEFAULT_RATE;

	r = strtol(s, &e, 10);
	if (r <= 0)
		return DEFAULT_RATE;
	while (*e == ' ')
		e++;

	if      (!*e)                      r *= 1;
	else if (!strcasecmp(e, "wpm"))    r *= 1;
	else if (!strcasecmp(e, "w/m"))    r *= 1;
	else if (!strcasecmp(e, "/m"))     r *= 1;
	else if (!strcasecmp(e, "wpmin"))  r *= 1;
	else if (!strcasecmp(e, "w/min"))  r *= 1;
	else if (!strcasecmp(e, "/min"))   r *= 1;
	else if (!strcasecmp(e, "wps"))    r *= 60;
	else if (!strcasecmp(e, "w/s"))    r *= 60;
	else if (!strcasecmp(e, "/s"))     r *= 60;
	else if (!strcasecmp(e, "wpsec"))  r *= 60;
	else if (!strcasecmp(e, "w/sec"))  r *= 60;
	else if (!strcasecmp(e, "/sec"))   r *= 60;
	else if (!strcasecmp(e, "hz"))     r *= 60;
	else
		return DEFAULT_RATE;

	return r;
}



int main(int argc, char *argv[])
{
	int dashed = 0;
	long rate = get_word_rate();
	char *file = NULL;
	char *arg;
	int fd = -1, ttyfd = -1, tty_configured = 0;
	struct termios stty;
	struct termios saved_stty;
	struct stat _attr;

	/* Check that we have a stdout. */
	if (fstat(STDOUT_FILENO, &_attr))
		t (errno == EBADF);

	/* Parse arguments. */
	argv0 = argv ? (argc--, *argv++) : "rq";
	while (argc) {
		if (!dashed && !strcmp(*argv, "--")) {
			dashed = 1;
			argv++;
			argc--;
		} else if (!dashed && **argv == '-') {
			arg = *argv++;
			argc--;
			for (arg++; *arg; arg++) {
				goto usage;
			}
		} else {
			if (file)
				goto usage;
			file = *argv++;
			argc--;
		}
	}

	/* Open file. */
	if (!file || !strcmp(file, "-")) {
		fd = STDIN_FILENO;
	} else {
		fd = open(file, O_RDONLY);
		t (fd == -1);
	}

	/* Get a readable file descriptor for the controlling terminal. */
	ttyfd = open("/dev/tty", O_RDONLY);
	t (ttyfd == -1);

	/* Configure terminal. */
	t (fprintf(stdout, "\033[?1049h\033[?25l") < 0);
	t (fflush(stdout));
	t (tcgetattr(ttyfd, &stty));
	saved_stty = stty;
	stty.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
	t (tcsetattr(ttyfd, TCSAFLUSH, &stty));
	tty_configured = 1;

	/* TODO Display file. */

	/* Restore terminal configurations. */
	tcsetattr(ttyfd, TCSAFLUSH, &saved_stty);
	fprintf(stdout, "\033[?25h\033[?1049l");
	fflush(stdout);
	tty_configured = 0;

	close(fd);
	close(ttyfd);
	return 0;

fail:
	perror(argv0);
	if (tty_configured) {
		tcsetattr(ttyfd, TCSAFLUSH, &saved_stty);
		fprintf(stdout, "\033[?25h\033[?1049l");
		fflush(stdout);
	}
	if (fd >= 0)
		close(fd);
	if (ttyfd >= 0)
		close(ttyfd);
	return 1;

usage:
	fprintf(stderr, "%s: Invalid arguments, see `man 1 rp'.\n", argv0);
	return 2;
}

