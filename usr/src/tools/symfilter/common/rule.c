/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)tools/symfilter/common/rule.c"

#include "symfilter.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <regex.h>

/*
 * Filtering rules.
 */

#define	ACTION_SEP		':'
#define	RULE_SEP		','

#define	RULE_MAXCOL		512
#define	COMMENT			'#'

#define	CONDITION_END(c)						\
	((c) == '\0' || isspace(c) || (c) == RULE_SEP ||		\
	 (c) == ACTION_SEP || (c) == '\n' || (c) == '\r')

typedef enum {
	FA_EXCLUDE = 0,		/* Exclude symbo */
	FA_INCLUDE = 1,		/* Include symbol */
	FA_SUBST = 2,		/* Substitution */
	FA_NOHASH = 3		/* Don't append symbol hash */
} fl_action_t;

/*
 * Rule condition.
 */
struct fl_cond;
typedef struct fl_cond	fl_cond_t;

typedef int	(*fl_eval_t)(fl_cond_t *cp, GElf_Sym *symp, char *name);

struct fl_cond {
	union {
		struct {
			char	*pat;	/* Regular expression */
			regex_t	*preg;	/* Compiled expression */
		} ru;
		uint_t	value;		/* Numerical value */
	} fc_u;
	fl_eval_t	fc_func;	/* Function to evaluate this rule */
	fl_cond_t	*fc_next;	/* Next condition */
};

#define	fc_pattern	fc_u.ru.pat
#define	fc_preg		fc_u.ru.preg
#define	fc_value	fc_u.value

/*
 * Filtering rule entry.
 */
struct frule;
typedef struct frule	frule_t;

struct frule {
	fl_action_t	fr_action;	/* Include or exclude symbol */
	fl_cond_t	*fr_cond;	/* Condition chain */
	int		fr_type;	/* New symbol type */
	int		fr_bind;	/* New symbol binding */
	frule_t		*fr_next;	/* Next rule */
};

/* Define symbol typess and bindings. */
typedef struct fr_symval {
	const char	*fs_name;	/* Symbolic name of this value */
	uint_t		fs_value;	/* Actual value */
} fr_symval_t;

/* Supported symbol types */
static fr_symval_t	SymTypes[] = {
	{"notype", STT_NOTYPE},
	{"object", STT_OBJECT},
	{"func", STT_FUNC},
	{"section", STT_SECTION},
	{"file", STT_FILE},
	{"common", STT_COMMON},
	{"tls", STT_TLS},
};

/* Supported symbol bindings */
static fr_symval_t	SymBinds[] = {
	{"local", STB_LOCAL},
	{"global", STB_GLOBAL},
	{"weak", STB_WEAK},
};

/* Convert name of type or binding to value. */
#define	SYMVAL_CONVERT(defs, name, value)				\
	do {								\
		int		__i;					\
		fr_symval_t	*__defp = (defs);			\
									\
		(value) = (uint_t)-1;					\
		for (__i = 0; __i < sizeof(defs) / sizeof(defs[0]);	\
		     __i++, __defp++) {					\
			if (strcmp(__defp->fs_name, name) == 0) {	\
				(value) = __defp->fs_value;		\
				break;					\
			}						\
		}							\
	} while (0)

#define	SYMVAL_CONVSTR(defs, value, name)				\
	do {								\
		int		__i;					\
		fr_symval_t	*__defp = (defs);			\
									\
		for (__i = 0; __i < sizeof(defs) / sizeof(defs[0]);	\
		     __i++, __defp++) {					\
			if (__defp->fs_value == (value)) {		\
				(name) = __defp->fs_name;		\
				break;					\
			}						\
		}							\
	} while (0)

/* Filtering rule chain */
static frule_t	*RuleChain;
static frule_t	**NextRule;

/* Substitution rule chain */
static frule_t	*SubChain;
static frule_t	**SubNextRule;

/* Symbol hash rule chain */
static frule_t	*SymHashChain;
static frule_t	**SymHashNextRule;

/* Internal Prototypes */
static frule_t	*rule_parse(char *pattern, char *file, int line);
static void	rule_error(char *file, int line, const char *fmt, ...);
static void	rule_dump(frule_t *frp);
static int	rule_eval_type(fl_cond_t *cp, GElf_Sym *symp, char *name);
static int	rule_eval_bind(fl_cond_t *cp, GElf_Sym *symp, char *name);
static int	rule_eval_regexp(fl_cond_t *cp, GElf_Sym *symp, char *name);

/*
 * void
 * rule_init(void)
 *	Initialize rule chain.
 */
void
rule_init(void)
{
	NextRule = &RuleChain;
	SubNextRule = &SubChain;
	SymHashNextRule = &SymHashChain;
}

/*
 * void
 * rule_add(char *rule)
 *	Append specified filtering rule at the tail of rule chain.
 */
void
rule_add(char *rule)
{
	(void)rule_parse(rule, NULL, 0);
}

/*
 * uint_t
 * rule_import(char *file)
 *	Import filter rules from the specified file.
 *
 * Calling/Exit State:
 *	rule_import() returns number of rules added to the chain.
 */
uint_t
rule_import(char *file)
{
	FILE	*fp;
	char	line[RULE_MAXCOL];
	uint_t	nrules = 0;
	int	lineno = 1;

	if ((fp = fopen(file, "r")) == NULL) {
		fatal(errno, "Can't open \"%s\"", file);
	}

	verbose(1, "Import rules from \"%s\"", file);
	for (; fgets(line, RULE_MAXCOL, fp) != NULL; lineno++) {
		char	*p, *endp;

		/* Ignore whitespace. */
		for (p = line; p < line + RULE_MAXCOL && isspace(*p); p++);
		if (p >= line + RULE_MAXCOL) {
			rule_error(file, lineno, "Too long line");
		}
		if (*p == '\0' || *p == '\n' || *p == '\r' || *p == COMMENT) {
			/* Ignore empty line or comment. */
			continue;
		}

		/* Eliminate comment in this line. */
		for (endp = p;
		     endp < line + RULE_MAXCOL && *endp != '\0' &&
			     *endp != '\r' && *endp != '\n' &&
			     *endp != COMMENT;
		     endp++);
		if (p >= line + RULE_MAXCOL) {
			rule_error(file, lineno, "Too long line");
		}

		/* Eliminate trainling whitespace. */
		for (; endp >= p && isspace(*endp); endp--);
		if (endp < line + RULE_MAXCOL - 1) {
			endp++;
		}
		*endp = '\0';

		/* Parse this line as a rule. */
		(void)rule_parse(p, file, lineno);
		nrules++;
	}

	(void)fclose(fp);
	return nrules;
}

/*
 * int
 * rule_eval(GElf_Sym *symp, char *name, int exclude)
 *	Evaluate filtering rule chain.
 *
 *	Rules in the chain are evaluated in chain order, and the first matched
 *	rule determines whether the specified symbol should be included or not.
 *
 *	If exclude is true, rule_eval() returns 0 if no rule is matched.
 *	Otherwise it returns 1.
 *
 * Calling/Exit State:
 *	rule_eval() returns 1 if the specified symbol should be included.
 *	Otherwise 0.
 */
int
rule_eval(GElf_Sym *symp, char *name, int exclude)
{
	frule_t	*frp;
	int	result = (!exclude);

	for (frp = RuleChain; frp != NULL; frp = frp->fr_next) {
		fl_cond_t	*cp;

		for (cp = frp->fr_cond;
		     cp != NULL && cp->fc_func(cp, symp, name);
		     cp = cp->fc_next);
		if (cp == NULL) {
			/* Matched. */
			result = (frp->fr_action == FA_INCLUDE);
			break;
		}
	}

	return result;
}

/*
 * void
 * rule_subst(GElf_Sym *symp, char *name)
 *	Evaluate substitution rule chain.
 *
 *	Rules in the chain are evaluated in chain order, and the first matched
 *	rule determines whether the specified symbol should be modified or not.
 *
 * Calling/Exit State:
 *	If no substitution rule is matched, symbol entry is not modified.
 */
void
rule_subst(GElf_Sym *symp, char *name)
{
	frule_t	*frp;

	for (frp = SubChain; frp != NULL; frp = frp->fr_next) {
		fl_cond_t	*cp;

		for (cp = frp->fr_cond;
		     cp != NULL && cp->fc_func(cp, symp, name);
		     cp = cp->fc_next);
		if (cp == NULL) {
			GElf_Word	type, bind;

			/* Do substitution. */
			type = GELF_ST_TYPE(symp->st_info);
			bind = GELF_ST_BIND(symp->st_info);
			if (frp->fr_type != -1) {
				type = frp->fr_type;
			}
			if (frp->fr_bind != -1) {
				type = frp->fr_bind;
			}

			verbose(2, "%s: subst: type:%d -> %d, bind:%d->%d",
				name,
				GELF_ST_TYPE(symp->st_info), type,
				GELF_ST_BIND(symp->st_info), bind);
			symp->st_info = GELF_ST_INFO(bind, type);
			break;
		}
	}
}

/*
 * int
 * rule_nohash(GElf_Sym *symp, char *name)
 *	Evaluate symbol hash rule chain.
 *
 *	Rules in the chain are evaluated in chain order, and the first matched
 *	rule determines whether the specified symbol should be added to
 *	symbol hash or not.
 *
 * Calling/Exit State:
 *	rule_eval() returns 1 if the specified symbol should NOT be added
 *	to the symbol hash table. Otherwise 0.
 */
int
rule_nohash(GElf_Sym *symp, char *name)
{
	frule_t	*frp;
	int	result = 0;

	for (frp = SymHashChain; frp != NULL; frp = frp->fr_next) {
		fl_cond_t	*cp;

		for (cp = frp->fr_cond;
		     cp != NULL && cp->fc_func(cp, symp, name);
		     cp = cp->fc_next);
		if (cp == NULL) {
			/* Matched. This symbol should not exported. */
			result = 1;
			break;
		}
	}

	return result;
}

/*
 * static frule_t *
 * rule_parse(char *rule)
 *	Parse filtering rule.
 */
static frule_t *
rule_parse(char *rule, char *file, int line)
{
	frule_t		*frp, ***nextrule;
	fl_cond_t	*cp, **cpp;
	char		*pat, *patsv, *sep, last = '\0';

	frp = (frule_t *)xmalloc(sizeof(*frp));
	frp->fr_next = NULL;
	patsv = pat = xstrdup(rule);

	if ((sep = strchr(pat, ACTION_SEP)) == NULL) {
		rule_error(file, line, "Invalid rule: \"%s\"", pat);
	}
	*sep = '\0';

	/* Determine action. */
	if (strcmp(pat, "inc") == 0) {
		frp->fr_action = FA_INCLUDE;
		nextrule = &NextRule;
	}
	else if (strcmp(pat, "exc") == 0) {
		frp->fr_action = FA_EXCLUDE;
		nextrule = &NextRule;
	}
	else if (strcmp(pat, "sub") == 0) {
		frp->fr_action = FA_SUBST;
		nextrule = &SubNextRule;
	}
	else if (strcmp(pat, "nohash") == 0) {
		frp->fr_action = FA_NOHASH;
		nextrule = &SymHashNextRule;
	}
	else {
		rule_error(file, line, "Invalid action: \"%s\": %s",
			   pat, rule);
	}
	pat = sep + 1;
	cpp = &(frp->fr_cond);

	/* CONSTANTCONDITION */
	while (1) {
		char	*p;

		cp = (fl_cond_t *)xmalloc(sizeof(*cp));
		*cpp = cp;
		cpp = &(cp->fc_next);

		/* Check type of condition. */
		if (strncmp(pat, "regexp:", 7) == 0) {
			cp->fc_func = rule_eval_regexp;
			pat += 7;
		}
		else if (strncmp(pat, "type:", 5) == 0) {
			cp->fc_func = rule_eval_type;
			pat += 5;
		}
		else if (strncmp(pat, "bind:", 5) == 0) {
			cp->fc_func = rule_eval_bind;
			pat += 5;
		}
		else {
			rule_error(file, line,
				   "Invalid condition type \"%s\": %s",
				   pat, rule);
		}

		/* Parse condition field. */
		for (p = pat; !CONDITION_END(*p); p++);
		if (p == pat) {
			rule_error(file, line, "Empty condition field: %s",
				   rule);
		}
		last = *p;
		*p = '\0';

		if (cp->fc_func == rule_eval_regexp) {
			regex_t	*preg;
			int	err;

			cp->fc_pattern = xstrdup(pat);

			/* Compile regular expression. */
			preg = (regex_t *)xmalloc(sizeof(*preg));
			err = regcomp(preg, pat, REG_EXTENDED|REG_NOSUB);
			if (err != 0) {
				char	buf[128];

				(void)regerror(err, preg, buf, sizeof(buf));
				(void)fprintf(stderr, "*** REGEX ERROR: %s\n",
					      buf);
				rule_error(file, line, "Invalid pattern \"%s\""
					   ": %s", pat, rule);
			}
			cp->fc_preg = preg;
		}
		else if (cp->fc_func == rule_eval_type) {
			/* Filtering by symbol type. */
			/* LINTED(E_CONSTANT_CONDITION) */
			SYMVAL_CONVERT(SymTypes, pat, cp->fc_value);
			if (cp->fc_value == (uint_t)-1) {
				rule_error(file, line, "Invalid symbol type "
					   "\"%s\": %s", pat, rule);
			}
		}
		else {
			/* Filtering by symbol binding. */
			/* LINTED(E_CONSTANT_CONDITION) */
			SYMVAL_CONVERT(SymBinds, pat, cp->fc_value);
			if (cp->fc_value == (uint_t)-1) {
				rule_error(file, line,
					   "Invalid symbol binding \"%s\": %s",
					   pat, rule);
			}
		}

		pat = p + 1;
		if (last != RULE_SEP) {
			/* End of rules. */
			break;
		}
	}
	cp->fc_next = NULL;

	if (frp->fr_action == FA_SUBST) {
		frp->fr_type = -1;
		frp->fr_bind = -1;

		if (last != ACTION_SEP) {
			rule_error(file, line,
				   "New symbol value is required: %s", rule);
		}

		/* Parse new symbol value. */
		/* CONSTANTCONDITION */
		while (1) {
			char	*p;

			for (p = pat; !CONDITION_END(*p); p++);
			if (p == pat) {
				rule_error(file, line, "Empty new value for "
					   "substitution: %s", rule);
			}
			last = *p;
			*p = '\0';
			if (strncmp(pat, "type=", 5) == 0) {
				pat += 5;
				/* LINTED(E_CONSTANT_CONDITION) */
				SYMVAL_CONVERT(SymTypes, pat, frp->fr_type);
				if (frp->fr_type == -1) {
					rule_error(file, line,
						   "Invalid symbol type "
						   "\"%s\": %s", pat, rule);
				}
			}
			else if (strncmp(pat, "bind=", 5) == 0) {
				pat += 5;
				/* LINTED(E_CONSTANT_CONDITION) */
				SYMVAL_CONVERT(SymBinds, pat, frp->fr_bind);
				if (frp->fr_bind == -1) {
					rule_error(file, line,
						   "Invalid symbol binding "
						   "\"%s\": %s", pat, rule);
				}
			}
			else {
				rule_error(file, line, "Invalid value for "
					   "substitution \"%s\": %s",
					   pat, rule);
			}

			pat = p + 1;
			if (last != RULE_SEP) {
				/* End of new value for substitution. */
				break;
			}
		}
	}

	xfree(patsv);

	if (Verbose > 0) {
		rule_dump(frp);
	}

	**nextrule = frp;
	*nextrule = &(frp->fr_next);

	return frp;
}

/*
 * static void
 * rule_error(char *file, int line, const char *fmt, ...)
 *	Report error of rule parser and die.
 */
static void
rule_error(char *file, int line, const char *fmt, ...)
{
	va_list	ap;

	if (file != NULL) {
		(void)fprintf(stderr, "%s: line %d: ", file, line);
	}
	(void)fprintf(stderr, "*** ERROR: ");
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)putc('\n', stderr);

	exit(1);
}

/*
 * static void
 * rule_dump(frule_t *frp)
 *	Print rule to stderr.
 */
static void
rule_dump(frule_t *frp)
{
	fl_cond_t	*cp;
	const char	*and = "";

	(void)fprintf(stderr, " + RULE: %s ",
		      (frp->fr_action == FA_INCLUDE) ? "<include>" :
		      (frp->fr_action == FA_EXCLUDE) ? "<exclude>" : "<subst>");
	for (cp = frp->fr_cond; cp != NULL; cp = cp->fc_next) {
		const char	*name;

		(void)fputs(and, stderr);

		if (cp->fc_func == rule_eval_regexp) {
			(void)fprintf(stderr, "regexp:\"%s\"", cp->fc_pattern);
		}
		else if (cp->fc_func == rule_eval_bind) {
			/* LINTED(E_CONSTANT_CONDITION) */
			SYMVAL_CONVSTR(SymBinds, cp->fc_value, name);
			(void)fprintf(stderr, "bind:%s", name);
		}
		else {
			/* LINTED(E_CONSTANT_CONDITION) */
			SYMVAL_CONVSTR(SymTypes, cp->fc_value, name);
			(void)fprintf(stderr, "type:%s", name);
		}
		and = " AND ";
	}

	if (frp->fr_action == FA_SUBST) {
		const char	*name, *sep = "";

		(void)fputs(", changes: ", stderr);
		if (frp->fr_type != -1) {
			/* LINTED(E_CONSTANT_CONDITION) */
			SYMVAL_CONVSTR(SymTypes, frp->fr_type, name);
			(void)fprintf(stderr, "type => %s", name);
			sep = ", ";
		}
		if (frp->fr_bind != -1) {
			/* LINTED(E_CONSTANT_CONDITION) */
			SYMVAL_CONVSTR(SymBinds, frp->fr_bind, name);
			(void)fprintf(stderr, "%sbinding => %s", sep, name);
		}
	}

	(void)putc('\n', stderr);
}

/*
 * static int
 * rule_eval_type(fl_cond_t *cp, GElf_Sym *symp, char *name)
 *	Evaluate symbol type rule.
 *
 * Calling/Exit State:
 *	rule_eval_type() returns 1 if the symbol type equals to fc_value.
 *	Otherwise 0.
 */
/* ARGSUSED2 */
static int
rule_eval_type(fl_cond_t *cp, GElf_Sym *symp, char *name)
{
	return (GELF_ST_TYPE(symp->st_info) == cp->fc_value);
}

/*
 * static int
 * rule_eval_bind(fl_cond_t *cp, GElf_Sym *symp, char *name)
 *	Evaluate symbol binding rule.
 *
 * Calling/Exit State:
 *	rule_eval_bind() returns 1 if the symbol binding equals to fc_value.
 *	Otherwise 0.
 */
/* ARGSUSED2 */
static int
rule_eval_bind(fl_cond_t *cp, GElf_Sym *symp, char *name)
{
	return (GELF_ST_BIND(symp->st_info) == cp->fc_value);
}

/*
 * static int
 * rule_eval_regexp(fl_cond_t *cp, GElf_Sym *symp, char *name)
 *	Evaluate regular expression rule.
 *
 * Calling/Exit State:
 *	rule_eval_regexp() returns 1 if the given symbol name matches the
 *	pattern in the fl_cond structure. Otherwise 0.
 */
/* ARGSUSED1 */
static int
rule_eval_regexp(fl_cond_t *cp, GElf_Sym *symp, char *name)
{
	return (regexec(cp->fc_preg, name, 0, NULL, 0) == 0);
}
