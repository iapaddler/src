/*	$OpenBSD: main.c,v 1.8 2002/06/14 21:35:00 todd Exp $	*/
/*	$NetBSD: main.c,v 1.3 1995/09/02 06:15:37 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: main.c,v 1.8 2002/06/14 21:35:00 todd Exp $";
#endif /* not lint */

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "error.h"
#include "pathnames.h"

int	nerrors = 0;
Eptr	er_head;
Eptr	*errors;

int	nfiles = 0;
Eptr	**files;	/* array of pointers into errors*/
int	language = INCC;

char	*currentfilename = "????";
char	im_on[] = _PATH_TTY;	/* my tty name */

boolean	query = FALSE;		/* query the operator if touch files */
boolean	notouch = FALSE;	/* don't touch ANY files */
boolean	piflag	= FALSE;	/* this is not pi */
boolean	terse	= FALSE;	/* Terse output */

char	*suffixlist = ".*";	/* initially, can touch any file */

int	errorsort();
void	onintr();
void	usage();

/*
 *	error [-I ignorename] [-n] [-q] [-t suffixlist] [-s] [-v] [infile]
 *	
 *	-T:	terse output
 *
 *	-I:	the following name, `ignorename' contains a list of
 *		function names that are not to be treated as hard errors.
 *		Default: ~/.errorsrc
 *
 *	-n:	don't touch ANY files!
 *
 *	-q:	The user is to be queried before touching each
 *		file; if not specified, all files with hard, non
 *		ignorable errors are touched (assuming they can be).
 *
 *	-t:	touch only files ending with the list of suffices, each
 *		suffix preceded by a dot.
 *		eg, -t .c.y.l
 *		will touch only files ending with .c, .y or .l
 *
 *	-s:	print a summary of the error's categories.
 *
 *	-v:	after touching all files, overlay vi(1), ex(1) or ed(1)
 *		on top of error, entered in the first file with
 *		an error in it, with the appropriate editor
 *		set up to use the "next" command to get the other
 *		files containing errors.
 *
 *	-p:	(obsolete: for older versions of pi without bug
 *		fix regarding printing out the name of the main file
 *		with an error in it)
 *		Take the following argument and use it as the name of
 *		the pascal source file, suffix .p
 *
 *	-S:	show the errors in unsorted order
 *		(as they come from the error file)
 *
 *	infile:	The error messages come from this file.
 *		Default: stdin
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *ignorename = 0;
	char **ed_argv;		/* return from touchfiles */
	int ch, ed_argc;
	boolean	show_errors = FALSE;
	boolean	Show_Errors = FALSE;
	boolean	pr_summary = FALSE;
	boolean	edit_files = FALSE;

	errorfile = stdin;
	while ((ch = getopt(argc, argv, "STnqsvI:t:")) != -1)
		switch (ch) {
		case 'S':
			Show_Errors = TRUE;
			break;
		case 'T':
			terse = TRUE;
			break;
		case 'n':
			notouch = TRUE;
			break;
		case 'q':
			query = TRUE;
			break;
		case 's':
			pr_summary = TRUE;
			break;
		case 'v':
			edit_files = TRUE;
			break;
		case 'I':
			ignorename = optarg;
			break;
		case 't':
			suffixlist = optarg;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (notouch)
		suffixlist = NULL;

	if (argc > 1) {
		if (argc > 3)
			errx(3, "Only takes 0 or 1 arguments.");
		if ((errorfile = fopen(argv[1], "r")) == NULL)
			err(4, "%s", argv[1]);
	}

	if ((queryfile = fopen(im_on, "r")) == NULL) {
		if (query) {
			errx(9, "Can't open \"%s\" to query the user.", im_on);
			exit(9);
		}
	}

	if (signal(SIGINT, onintr) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, onintr) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);

	getignored(ignorename);
	eaterrors(&nerrors, &errors);

	if (Show_Errors)
		printerrors(TRUE, nerrors, errors);

	qsort(errors, nerrors, sizeof(Eptr), errorsort);

	if (show_errors)
		printerrors(FALSE, nerrors, errors);

	findfiles(nerrors, errors, &nfiles, &files);

#define P(msg, arg) fprintf(stdout, msg, arg)

	if (pr_summary) {
		if (nunknown)
			P("%d Errors are unclassifiable.\n", nunknown);
		if (nignore)
			P("%d Errors are classifiable, but totally discarded.\n",nignore);
		if (nsyncerrors)
			P("%d Errors are synchronization errors.\n", nsyncerrors);
		if (nignore)
			P("%d Errors are discarded because they refer to sacrosinct files.\n", ndiscard);
		if (nnulled)
			P("%d Errors are nulled because they refer to specific functions.\n", nnulled);
		if (nnonspec)
			P("%d Errors are not specific to any file.\n", nnonspec);
		if (nthisfile)
			P("%d Errors are specific to a given file, but not to a line.\n", nthisfile);
		if (ntrue)
			P("%d Errors are true errors, and can be inserted into the files.\n", ntrue);
	}

	filenames(nfiles, files);
	fflush(stdout);

	if (touchfiles(nfiles, files, &ed_argc, &ed_argv) && edit_files)
		forkvi(ed_argc, ed_argv);
	exit(0);
}

void
forkvi(argc, argv)
	int	argc;
	char	**argv;
{
	if (query) {
		switch (inquire(terse ? "Edit? " :
		    "Do you still want to edit the files you touched? ")) {
		case Q_NO:
		case Q_no:
			return;
		default:
			break;
		}
	}

	/*
	 *	ed_agument's first argument is
	 *	a vi/ex compatible search argument
	 *	to find the first occurance of ###
	 */
	try("vi", argc, argv);
	try("ex", argc, argv);
	try("ed", argc - 1, argv + 1);
	fprintf(stdout, "Can't find any editors.\n");
}

void
try(name, argc, argv)
	char *name;
	int argc;
	char **argv;
{
	argv[0] = name;
	wordvprint(stdout, argc, argv);
	fprintf(stdout, "\n");
	fflush(stderr);
	fflush(stdout);
	sleep(2);

	if (freopen(im_on, "r", stdin) == NULL)
		return;
	if (freopen(im_on, "w", stdout) == NULL)
		return;

	execvp(name, argv);
}

int errorsort(epp1, epp2)
	Eptr *epp1, *epp2;
{
	reg Eptr ep1, ep2;
	int order;

	/*
	 *	Sort by:
	 *	1) synchronization, non specific, discarded errors first;
	 *	2) nulled and true errors last
	 *	    a) grouped by similar file names
	 *	    b) grouped in ascending line number
	 */
	ep1 = *epp1; ep2 = *epp2;
	if (ep1 == 0 || ep2 == 0)
		return (0);

	if ((NOTSORTABLE(ep1->error_e_class)) ^
	    (NOTSORTABLE(ep2->error_e_class))) {
		return (NOTSORTABLE(ep1->error_e_class) ? -1 : 1);
	}

	if (NOTSORTABLE(ep1->error_e_class))	/* then both are */
		return (ep1->error_no - ep2->error_no);

	order = strcmp(ep1->error_text[0], ep2->error_text[0]);
	if (order == 0)
		return (ep1->error_line - ep2->error_line);

	return (order);
}

void
usage()
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-STnsqv] [-I ignorefile] [-t suffixlist] [name]\n",
	    __progname);
	exit(1);
}
