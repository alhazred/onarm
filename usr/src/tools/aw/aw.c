/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Wrapper for the GNU assembler to make it accept the Sun assembler
 * arguments where possible.
 *
 * There are several limitations; the Sun assembler takes multiple
 * source files, we only take one.
 *
 * -b, -s, -xF, -T plain not supported.
 * -S isn't supported either, because while GNU as does generate
 * listings with -a, there's no obvious mapping between sub-options.
 * -K pic, -K PIC not supported either, though it's not clear what
 * these actually do ..
 * -Qy (not supported) adds a string to the .comment section
 * describing the assembler version, while
 * -Qn (supported) suppresses the string (also the default).
 *
 * We also add '-#' support to see invocation lines..
 * We also add '-xarch=amd64' in case we need to feed the assembler
 * something different (or in case we need to invoke a different binary
 * altogether!)
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static const char *progname;
static int verbose;

struct aelist {
	int ael_argc;
	struct ae {
		struct ae *ae_next;
		char *ae_arg;
	} *ael_head, *ael_tail;
};

#ifndef	DEFAULT_AS_NAME
#define	DEFAULT_AS_NAME		"gas"
#endif	/* !DEFAULT_AS_NAME */

#ifndef	DEFAULT_CPP_NAME
#define	DEFAULT_CPP_NAME	"cpp"
#endif	/* !DEFAULT_CPP_NAME */

static struct aelist *
newael(void)
{
	return (calloc(sizeof (struct aelist), 1));
}

static void
newae(struct aelist *ael, const char *arg)
{
	struct ae *ae;

	ae = calloc(sizeof (*ae), 1);
	ae->ae_arg = strdup(arg);
	if (ael->ael_tail == NULL)
		ael->ael_head = ae;
	else
		ael->ael_tail->ae_next = ae;
	ael->ael_tail = ae;
	ael->ael_argc++;
}

static void
fixae_arg(struct ae *ae, const char *newarg)
{
	free(ae->ae_arg);
	ae->ae_arg = strdup(newarg);
}

static char **
aeltoargv(struct aelist *ael)
{
	struct ae *ae;
	char **argv;
	int argc;

	argv = calloc(sizeof (*argv), ael->ael_argc + 1);

	for (argc = 0, ae = ael->ael_head; ae; ae = ae->ae_next, argc++) {
		argv[argc] = ae->ae_arg;
		if (ae == ael->ael_tail)
			break;
	}

	return (argv);
}

static int
error(const char *arg)
{
	(void) fprintf(stderr,
	    "%s: as->gas mapping failed at or near arg '%s'\n", progname, arg);
	return (2);
}

static int
usage(const char *arg)
{
	if (arg != NULL)
		(void) fprintf(stderr, "error: %s\n", arg);
	(void) fprintf(stderr, "Usage: %s [-V] [-#]\n"
	    "\t[-xarch=architecture]\n"
	    "\t[-o objfile] [-L]\n"
	    "\t[-P [[-Ipath] [-Dname] [-Dname=def] [-Uname]]...]\n"
	    "\t[-m] [-n] file.s ...\n", progname);
	return (3);
}

static void
copyuntil(FILE *in, FILE *out, int termchar)
{
	int c;

	while ((c = fgetc(in)) != EOF) {
		if (out && fputc(c, out) == EOF)
			exit(1);
		if (c == termchar)
			break;
	}
}

/*
 * The idea here is to take directives like this emitted
 * by cpp:
 *
 *	# num
 *
 * and convert them to directives like this that are
 * understood by the GNU assembler:
 *
 *	.line num
 *
 * and similarly:
 *
 *	# num "string" optional stuff
 *
 * is converted to
 *
 *	.line num
 *	.file "string"
 *
 * While this could be done with a sequence of sed
 * commands, this is simpler and faster..
 */
static pid_t
filter(int pipein, int pipeout)
{
	pid_t pid;
	FILE *in, *out;

	if (verbose)
		(void) fprintf(stderr, "{#line filter} ");

	switch (pid = fork()) {
	case 0:
		if (dup2(pipein, 0) == -1 ||
		    dup2(pipeout, 1) == -1) {
			perror("dup2");
			exit(1);
		}
		closefrom(3);
		break;
	case -1:
		perror("fork");
	default:
		return (pid);
	}

	in = fdopen(0, "r");
	out = fdopen(1, "w");

	while (!feof(in)) {
		int c, num;

		switch (c = fgetc(in)) {
		case '#':
			switch (fscanf(in, " %d", &num)) {
			case 0:
				/*
				 * discard comment lines completely
				 * discard ident strings completely too.
				 * (GNU as politely ignores them..)
				 */
				copyuntil(in, NULL, '\n');
				break;
			default:
				(void) fprintf(stderr, "fscanf botch?");
				/*FALLTHROUGH*/
			case EOF:
				exit(1);
				/*NOTREACHED*/
			case 1:
				/*
				 * This line has a number at the beginning;
				 * if it has a string after the number, then
				 * it's a filename.
				 */
				if (fgetc(in) == ' ' && fgetc(in) == '"') {
					(void) fprintf(out, "\t.file \"");
					copyuntil(in, out, '"');
					(void) fputc('\n', out);
				}
				(void) fprintf(out, "\t.line %d\n", num - 1);
				/*
				 * discard the rest of the line
				 */
				copyuntil(in, NULL, '\n');
				break;
			}
			break;
		case '\n':
			/*
			 * preserve newlines
			 */
			(void) fputc(c, out);
			break;
		case EOF:
			/*
			 * don't write EOF!
			 */
			break;
		default:
			/*
			 * lines that don't begin with '#' are copied
			 */
			(void) fputc(c, out);
			copyuntil(in, out, '\n');
			break;
		}

		if (ferror(out))
			exit(1);
	}

	exit(0);
	/*NOTREACHED*/
}

static pid_t
invoke(char **argv, int pipein, int pipeout)
{
	pid_t pid;

	if (verbose) {
		char **dargv = argv;

		(void)fprintf(stderr, "+ ");
		while (*dargv)
			(void) fprintf(stderr, "%s ", *dargv++);
	}

	switch (pid = fork()) {
	case 0:
		if (pipein >= 0 && dup2(pipein, 0) == -1) {
			perror("dup2");
			exit(1);
		}
		if (pipeout >= 0 && dup2(pipeout, 1) == -1) {
			perror("dup2");
			exit(1);
		}
		closefrom(3);
		(void) execvp(argv[0], argv);
		perror("execvp");
		(void) fprintf(stderr, "%s: couldn't run %s\n",
		    progname, argv[0]);
		break;
	case -1:
		perror("fork");
	default:
		return (pid);
	}
	exit(2);
	/*NOTREACHED*/
}

static int
pipeline(char **ppargv, char **asargv)
{
	int pipedes[4];
	int active = 0;
	int rval = 0;
	pid_t pid_pp, pid_f, pid_as;

	if (pipe(pipedes) == -1 || pipe(pipedes + 2) == -1) {
		perror("pipe");
		return (4);
	}

	if ((pid_pp = invoke(ppargv, -1, pipedes[0])) > 0)
		active++;

	if (verbose)
		(void) fprintf(stderr, "| ");

	if ((pid_f = filter(pipedes[1], pipedes[2])) > 0)
		active++;

	if (verbose)
		(void) fprintf(stderr, "| ");

	if ((pid_as = invoke(asargv, pipedes[3], -1)) > 0)
		active++;

	if (verbose) {
		(void) fprintf(stderr, "\n");
		(void) fflush(stderr);
	}

	closefrom(3);

	if (active != 3)
		return (5);

	while (active != 0) {
		pid_t pid;
		int stat;

		if ((pid = wait(&stat)) == -1) {
			rval++;
			break;
		}

		if (!WIFEXITED(stat))
			continue;

		if (pid == pid_pp || pid == pid_f || pid == pid_as) {
			active--;
			if (WEXITSTATUS(stat) != 0)
				rval++;
		}
	}

	return (rval);
}

int
main(int argc, char *argv[])
{
	struct aelist *cpp = NULL;
	struct aelist *m4 = NULL;
	struct aelist *as = newael();
	char **asargv;
	char *outfile = NULL;
	char *srcfile = NULL;
	const char *as_dir, *as64_dir, *m4_dir, *m4_lib_dir, *cpp_dir;
	char *as_pgm, *as64_pgm, *m4_pgm, *m4_cmdefs, *cpp_pgm;
	int as64 = 0;
	int code;
	const char	*as_name, *cpp_name;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	/*
	 * Helpful when debugging, or when changing tool versions..
	 */
	if ((as_dir = getenv("AW_AS_DIR")) == NULL)
		as_dir = DEFAULT_AS_DIR;	/* /usr/sfw/bin */
	if ((as_name = getenv("AW_AS_NAME")) == NULL) {
		as_name = DEFAULT_AS_NAME;
	}
	as_pgm = malloc(strlen(as_dir) + strlen(as_name) + 2);
	(void) sprintf(as_pgm, "%s/%s", as_dir, as_name);

	if ((as64_dir = getenv("AW_AS64_DIR")) == NULL)
		as64_dir = DEFAULT_AS64_DIR;	/* /usr/sfw/bin */
	as64_pgm = malloc(strlen(as64_dir) + strlen("/gas") + 1);
	(void) sprintf(as64_pgm, "%s/gas", as64_dir);

	if ((m4_dir = getenv("AW_M4_DIR")) == NULL)
		m4_dir = DEFAULT_M4_DIR;	/* /usr/ccs/bin */
	m4_pgm = malloc(strlen(m4_dir) + strlen("/m4") + 1);
	(void) sprintf(m4_pgm, "%s/m4", m4_dir);

	if ((m4_lib_dir = getenv("AW_M4LIB_DIR")) == NULL)
		m4_lib_dir = DEFAULT_M4LIB_DIR;	/* /usr/ccs/lib */
	m4_cmdefs = malloc(strlen(m4_lib_dir) + strlen("/cmdefs") + 1);
	(void) sprintf(m4_cmdefs, "%s/cmdefs", m4_lib_dir);

	if (argc > 1 && strncmp(*(argv + 1), "-cpp=", 5) == 0) {
		/* Use specified path as cpp. */
		cpp_name = *(argv + 1) + 5;
		cpp_pgm = strdup(cpp_name);
		argc--;
		argv++;
	}
	else {
		if ((cpp_dir = getenv("AW_CPP_DIR")) == NULL)
			cpp_dir = DEFAULT_CPP_DIR;	/* /usr/ccs/lib */
		if ((cpp_name = getenv("AW_CPP_NAME")) == NULL) {
			cpp_name = DEFAULT_CPP_NAME;
		}
		cpp_pgm = malloc(strlen(cpp_dir) + strlen(cpp_name) + 2);
		(void) sprintf(cpp_pgm, "%s/%s", cpp_dir, cpp_name);
	}

	newae(as, as_pgm);
	newae(as, "--warn");
	newae(as, "--fatal-warnings");
	newae(as, "--traditional-format");

#ifndef	WORKING_DOT_WORD
	/*
	 * This is a support hack to rewrite code for the compiler
	 * which should probably cause an assembler programmer to recode
	 * - so, generate a warning in this case.
	 */
	newae(as, "-K");
#endif	/* !WORKING_DOT_WORD */

	/*
	 * Walk the argument list, translating as we go ..
	 */
	while (--argc > 0) {
		char *arg;
		int arglen;

		arg = *++argv;
		arglen = strlen(arg);

		if (*arg != '-') {
			char *filename;

			/*
			 * filenames ending in '.s' are taken to be
			 * assembler files, and provide the default
			 * basename of the output file.
			 *
			 * other files are passed through to the
			 * preprocessor, if present, or to gas if not.
			 */
			filename = arg;
			if (arglen > 2 &&
			    strcmp(arg + arglen - 2, ".s") == 0) {
				/*
				 * Though 'as' allows multiple assembler
				 * files to be processed in one invocation
				 * of the assembler, ON only processes one
				 * file at a time, which makes things a lot
				 * simpler!
				 */
				if (srcfile == NULL)
					srcfile = arg;
				else
					return (usage(
					    "one assembler file at a time"));

				/*
				 * If we haven't seen a -o option yet,
				 * default the output to the basename
				 * of the input, substituting a .o on the end
				 */
				if (outfile == NULL) {
					char *argcopy;

					argcopy = strdup(arg);
					argcopy[arglen - 1] = 'o';

					if ((outfile = strrchr(
					    argcopy, '/')) == NULL)
						outfile = argcopy;
					else
						outfile++;
				}
			}
			if (cpp)
				newae(cpp, filename);
			else if (m4)
				newae(m4, filename);
			else
				newae(as, filename);
			continue;
		} else
			arglen--;

		switch (arg[1]) {
		case 'K':
			/*
			 * -K pic
			 * -K PIC
			 */
			if (arglen == 1) {
				if ((arg = *++argv) == NULL || *arg == '\0')
					return (usage("malformed -K"));
				argc--;
			} else {
				arg += 2;
			}
			if (strcmp(arg, "PIC") != 0 && strcmp(arg, "pic") != 0)
				return (usage("malformed -K"));
			break;		/* just ignore -Kpic for gcc */
		case 'Q':
			if (strcmp(arg, "-Qn") == 0)
				break;
			/*FALLTHROUGH*/
		case 'b':
		case 's':
		case 'T':
			/*
			 * -b	Extra symbol table for source browser ..
			 *	not relevant to gas, thus should error.
			 * -s	Put stabs in .stabs section not stabs.excl
			 *	not clear if there's an equivalent
			 * -T	4.x migration option
			 */
		default:
			return (error(arg));
		case 'x':
			/*
			 * Accept -xarch special case to invoke alternate
			 * assemblers or assembler flags for different
			 * architectures.
			 */
			if (strcmp(arg, "-xarch=amd64") == 0 ||
			    strcmp(arg, "-xarch=generic64") == 0) {
				as64++;
				fixae_arg(as->ael_head, as64_pgm);
				break;
			}
			/*
			 * XX64: Is this useful to gas?
			 */
			if (strcmp(arg, "-xmodel=kernel") == 0)
				break;

			if (strcmp(arg, "-xarch=arm") == 0) {
				newae(as, "-mcpu=arm1136jf-s");
				break;
			}
			else if (strcmp(arg, "-xarch=mpcore") == 0) {
				newae(as, "-mcpu=mpcore");
				break;
			}
			else if (strcmp(arg, "-xarch=mpcorenovfp") == 0) {
				newae(as, "-mcpu=mpcorenovfp");
				break;
			}

			/*
			 * -xF	Generates performance analysis data
			 *	no equivalent
			 */
			return (error(arg));
		case 'V':
			newae(as, arg);
			break;
		case '#':
			verbose++;
			break;
		case 'L':
			newae(as, "--keep-locals");
			break;
		case 'n':
			newae(as, "--no-warn");
			break;
		case 'o':
			if (arglen != 1)
				return (usage("bad -o flag"));
			if ((arg = *++argv) == NULL || *arg == '\0')
				return (usage("bad -o flag"));
			outfile = arg;
			argc--;
			arglen = strlen(arg + 1);
			break;
		case 'm':
			if (cpp)
				return (usage("-m conflicts with -P"));
			if (m4 == NULL) {
				m4 = newael();
				newae(m4, m4_pgm);
				newae(m4, m4_cmdefs);
			}
			break;
		case 'P':
			if (m4)
				return (usage("-P conflicts with -m"));
			if (cpp == NULL) {
				cpp = newael();
				newae(cpp, cpp_pgm);
				newae(cpp, "-D__GNUC_AS__");
			}
			break;
		case 'D':
		case 'U':
			if (cpp)
				newae(cpp, arg);
			else if (m4)
				newae(m4, arg);
			else
				newae(as, arg);
			break;
		case 'I':
			if (cpp)
				newae(cpp, arg);
			else
				newae(as, arg);
			break;
		case '-':	/* a gas-specific option */
			newae(as, arg);
			break;
		case '_':
			if (strncmp(arg, "-_cpp=", 6) == 0 && cpp) {
				/* cpp-specific option */
				newae(cpp, arg + 6);
			}
			else if (strncmp(arg, "-_m4=", 5) == 0 && m4) {
				/* m4-specific option */
				newae(m4, arg + 5);
			}
			else if (strncmp(arg, "-_gas=", 6) == 0) {
				/* gas-specific option*/
				newae(as, arg + 6);
			}
			else {
				return error(arg);
			}
			break;
		}
	}

#if	defined(__i386) && !defined(TARGET_arm)
	if (as64)
		newae(as, "--64");
	else
		newae(as, "--32");
#endif	/* defined(__i386) && !defined(TARGET_arm) */

	if (srcfile == NULL)
		return (usage("no source file(s) specified"));
	if (outfile == NULL)
		outfile = "a.out";
	newae(as, "-o");
	newae(as, outfile);

	asargv = aeltoargv(as);
	if (cpp) {
#if defined(__sparc)
		newae(cpp, "-Dsparc");
		newae(cpp, "-D__sparc");
		if (as64)
			newae(cpp, "-D__sparcv9");
		else
			newae(cpp, "-D__sparcv8");
#elif defined(__arm) || defined(TARGET_arm)
		newae(cpp, "-D__arm");
		newae(cpp, "-Darm");
#elif defined(__i386) || defined(__x86)
		if (as64) {
			newae(cpp, "-D__x86_64");
			newae(cpp, "-D__amd64");
		} else {
			newae(cpp, "-Di386");
			newae(cpp, "-D__i386");
		}
#else
#error	"need isa-dependent defines"
#endif
		code = pipeline(aeltoargv(cpp), asargv);
	} else if (m4)
		code = pipeline(aeltoargv(m4), asargv);
	else {
		/*
		 * XXX	should arrange to fork/exec so that we
		 *	can unlink the output file if errors are
		 *	detected..
		 */
		(void) execvp(asargv[0], asargv);
		perror("execvp");
		(void) fprintf(stderr, "%s: couldn't run %s\n",
		    progname, asargv[0]);
		code = 7;
	}
	if (code != 0)
		(void) unlink(outfile);
	return (code);
}
