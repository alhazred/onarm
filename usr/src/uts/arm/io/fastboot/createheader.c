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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_MODNAME 256
#define MAX_MODNUM  512
#define MAX_PCNT     20

/* Error Code */
#define ERR_OPENFILE        -1
#define ERR_FORMAT_SYMBOLE1 -2
#define ERR_FORMAT_SYMBOLE2 -3
#define ERR_MEMALLOC        -4
#define ERR_INTERNAL        -5

/* module information */
typedef struct modinfo{
	char name[MAX_MODNAME];
	int major;
	int pcnt;
	struct modinfo *parents[MAX_PCNT];
	int parents_index[MAX_PCNT];
	struct modinfo *next;
	int index;
}modinfo_t;


static int read_moduledependence(char *);
static int read_nametomajor(char *);
static int write_header(char *outfile);
static modinfo_t *search_module_name(char *);
static modinfo_t *search_module_major(int);
static void create_modulelist(void);


static modinfo_t init_queue[MAX_MODNUM];    /* array of module */
static modinfo_t *head;                /* head of modinfo_t list  */
static modinfo_t *tail;                /* tail of modinfo_t list  */

static int max_index;     /* The number of module with major */
static int module_number; /* The number of all module */
static int index_minor;   /* The number of module which doesn't have major */



/*
 * static int
 * read_moduledependence(char *filename)
 *      The read_moduledependence() function read module_depends file
 *      and create module list.
 *
 * Calling/Exit State:
 *      - return value
 *            0 : success
 *           -1 : fail to open filename
 *           -2 : format error
 *           -3 : format error
 *           -4 : memory allocate error
 *
 *      - argument
 *           filename  File path of module_depends
 */
static int
read_moduledependence(char *filename)
{
	char buff[MAX_MODNAME+2];
	modinfo_t *module = NULL, *tmp_mod = NULL;
	int rtn = 0;
	FILE *fp;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		rtn = ERR_OPENFILE;
		goto error;
	}

	while (fgets(buff, sizeof(buff), fp) != NULL) {
		char index;
		char *separat[2];

		separat[0] = strtok(buff, " \t\n");
		separat[1] = strtok(NULL, " \t\n");

		/* search character from top of stream.                     */
		/* if first character does not equals 'M'/'n', it is error. */
		/* if "M A B" , B is neglect.                               */

		/* first character check */
		if (*separat[0] == 'M') {
			index = 'M';
		} else if (*separat[0] == 'n') {
			index = 'n';
		} else {
			rtn = ERR_FORMAT_SYMBOLE1;
			goto error;
		}

		module = search_module_name(separat[1]);
		if (module == NULL) {
			rtn = ERR_MEMALLOC;
			goto error;
		}

		if (index == 'M') {
			tmp_mod = module;
		} else {
			if (tmp_mod == NULL) {
				rtn = ERR_FORMAT_SYMBOLE2;
				goto error;
			}
			tmp_mod->parents[tmp_mod->pcnt++] = module;
			
			/* Check the upper limit of parents array. */
			if (tmp_mod->pcnt >= MAX_PCNT) {
				rtn = ERR_INTERNAL;
				goto error;
			}
		}
	}

error:
	if (fp != NULL) {
		(void)fclose(fp);
	}
	return rtn;
}

/*
 * static int
 * read_nametomajor(char *filename)
 *      The read_nametomajor() function read name_to_major file
 *      and set major number as module.
 *
 * Calling/Exit State:
 *      - return value
 *            0 : success
 *           -1 : fail to open filename
 *           -6 : memory allocate error
 *
 *      - argument
 *           filename  File path of name_to_major
 *
 * Remarks:
 *      A format error of name_to_major file isn't detected.
 */
static int
read_nametomajor(char *filename)
{
	FILE *fp;
	char buff[MAX_MODNAME];
	int rtn = 0;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		rtn = ERR_OPENFILE;
		goto error;
	}

	while (fgets(buff, sizeof(buff), fp) != NULL) {
		int major = -1;
		char *separat[2];
		modinfo_t *module;

		if (*buff == '#') {
			/* Skip comments. */
			continue;
		}

		separat[0] = strtok(buff, " \t\n");
		separat[1] = strtok(NULL, " \t\n");

		if (separat[1] == NULL) {
			continue;
		}
		major = atoi(separat[1]);

		module = search_module_name(separat[0]);
		if (module == NULL) {
			rtn = ERR_MEMALLOC;
			goto error;
		}

		module->major = major;
	}

error:
	if (fp != NULL) {
		(void)fclose(fp);
	}

	return rtn;
}


/*
 * static int
 * write_header(char *outfile)
 *	The write_header() function dumps initialization queue into the
 *	given file. If outfile is NULL, file contents will be dumped to the
 *	standard output.
 *
 * Calling/Exit State:
 *	Upon successful completion, write_header() returns zero.
 *	Otherwise it returns error code. (ERR_xxx)
 *
 * Remarks:
 *	The output file is always overwritten if outfile exists.
 *
 *	The value of kmutex member in io_modinfo_t will be dumped as
 *	"NULL" if _LP64 is defined. If not defined, it will be dumped as
 *	"NULL, NULL".
 */
static int
write_header(char *outfile)
{
	int i, j, rtn = 0;
	int number = module_number;
	FILE *fp;

	if (outfile == NULL) {
		fp = stdout;
	}
	else {
		int	fd;

		if ((fd = open(outfile, O_CREAT|O_TRUNC|O_WRONLY, 0644))
		    == -1) {
			fp = NULL;
			rtn = ERR_OPENFILE;
			goto error;
		}
		if ((fp = fdopen(fd, "w")) == NULL) {
			(void)close(fd);
			rtn = ERR_OPENFILE;
			goto error;
		}
	}

	(void)fprintf(fp, "#ifndef\t_IO_FASTBOOT_H\n"
		      "#error\tio_fastboot.h must be included in advance.\n"
		      "#endif\t/* !_IO_FASTBOOT_H */\n\n");

	(void)fprintf(fp, "io_modinfo_t modinfo_array[%d] = {\n", number);
	(void)fprintf(fp, "#ifdef _LP64\n");

	for (i = 0; i < number; i++) {
		(void)fprintf(fp, "\t{");

		/* char* name */
		(void)fprintf(fp, "\"%s\",", init_queue[i].name);

		/* char status */
		(void)fprintf(fp, "0x00,");

		/* int major */
		(void)fprintf(fp, "%d,", init_queue[i].major);

		/* kmutex_t mutex */
		(void)fprintf(fp, "NULL,");

		/* int pcnt */
		(void)fprintf(fp, "%d,", init_queue[i].pcnt);

		(void)fprintf(fp, "\"");
		for (j = 0; j < init_queue[i].pcnt; j++) {
			(void)fprintf(fp, "%d", init_queue[i].parents_index[j]);
			if (j != init_queue[i].pcnt-1) {
				(void)fprintf(fp, ",");
			}
		}
		if (init_queue[i].pcnt > 0) {
			(void)fprintf(fp, ";\",");
		} else {
			(void)fprintf(fp, "\",");
		}
		(void)fprintf(fp, "NULL");	/* io_modinfo_t* qnext */
		(void)fprintf(fp, "}");
		
		if (i != number - 1) {
			(void)fprintf(fp, ",\n");
		}
	}

	(void)fprintf(fp, "\n#else\n");

	for (i = 0; i < number; i++) {
		(void)fprintf(fp, "\t{");

		/* char* name */
		(void)fprintf(fp, "\"%s\",", init_queue[i].name);

		/* char status */
		(void)fprintf(fp,"0x00,");

		/* int major */
		(void)fprintf(fp, "%d,", init_queue[i].major);

		/* kmutex_t mutex */
		(void)fprintf(fp, "NULL,NULL,");

		/* int pcnt */
		(void)fprintf(fp, "%d,", init_queue[i].pcnt);

		(void)fprintf(fp, "\"");
		for (j = 0; j < init_queue[i].pcnt; j++) {
			(void)fprintf(fp, "%d",init_queue[i].parents_index[j]);
			if (j != init_queue[i].pcnt-1) {
				(void)fprintf(fp, ",");
			}
		}
		if (init_queue[i].pcnt > 0) {
			(void)fprintf(fp, ";\",");
		} else {
			(void)fprintf(fp, "\",");
		}
		(void)fprintf(fp, "NULL"); /* io_modinfo_t* qnext */
		(void)fprintf(fp, "}");

		if (i != number - 1) {
			(void)fprintf(fp, ",\n");
		}
	}
	(void)fprintf(fp, "\n#endif");
	(void)fprintf(fp, "\n};\n\n\n");

	(void)fprintf(fp, "#define IO_TAIL %d\n", number);
	(void)fprintf(fp, "#define IO_BOUND %d\n", max_index);

error:
	if (outfile != NULL) {
		if (fp != NULL) {
			(void)fclose(fp);
		}
		if (rtn != 0) {
			(void)unlink(outfile);
		}
	}

	return rtn;
}


/*
 * static modinfo_t*
 * search_module_name(char *name)
 *      The search_module_name() function search a modinfo_t from module list
 *      by name. If a modinfo_t isn't found, It's created.
 *
 * Calling/Exit State:
 *      - return value
 *            Upon successful completion, search_module_name() returns
 *            a pointer to the modinfo_t. Otherwise, a null pointer is 
 *            returned.
 *           
 *      - argument
 *           name  module name.
 */
static modinfo_t*
search_module_name(char *name)
{
	modinfo_t *module = head;

	if (name == NULL) {
		return NULL;
	}
	/* search module list from head. */
	for ( ; module != NULL; module = module->next) {
		if (strcmp(module->name, name) == 0) {
			return module;
		}
	}

	/* if modinfo_t not found, allocate new area. */
	module = (modinfo_t *)malloc(sizeof(modinfo_t));
	if (module == NULL) {
		return NULL;
	}

	(void)memset(module, '\0', sizeof(modinfo_t));
	(void)memcpy(module->name, name, strlen(name));
	module->pcnt = 0;
	module->major = -1;
	module->index = -1;

	/* set new struct to module list. */
	if (head == NULL){
		head = module;
	} else {
		tail->next = module;
	}
	tail = module;

	return module;
}

/*
 * static modinfo_t*
 * search_module_major(int major)
 *      The search_module_major() function search a modinfo_t from module list
 *      by major number.
 *
 * Calling/Exit State:
 *
 *      - return value
 *            Upon successful completion, search_module_name() returns
 *            a pointer to the modinfo_t. Otherwise, a null pointer is 
 *            returned.
 *
 *      - argument
 *           major  major number
 */
static modinfo_t*
search_module_major(int major)
{
	modinfo_t *module = head;

	for (; module != NULL; module = module->next) {
		if (module->major == major) {
			return module;
		}
	}

	return NULL;
}


/*
 * int
 * store_depmodule(modinfo_t *mod)
 *      The store_depmodule() function store the dependence module
 *      on init_queue.
 *
 * Calling/Exit State:
 *      - argument
 *           module  The pointer to the io_modinfo structure.
 */
static void
store_depmodule(modinfo_t *module)
{
	int i;

	for (i = 0; i < module->pcnt; i++) {
		/* have major number ? / already in */
		if (module->parents[i]->major == -1 && 
		              module->parents[i]->index == -1) {
			module->parents[i]->index = max_index+index_minor;
			init_queue[max_index+index_minor]
			                          = *(module->parents[i]);
			index_minor++;
			store_depmodule(module->parents[i]);
		}
	}
}


/*
 * static void
 * create_modulelist()
 *      The create_modulelist() function sort a modinfo_t major number order
 *      and make initialization queue.
 */
static void
create_modulelist()
{
	modinfo_t *module;
	int modnum_major = 0, index = 0, i, j;
	int major;

	/* count the driver's number with the major number */
	for (major = 0; major < MAX_MODNUM; major++) {
		module = search_module_major(major);
		if (module == NULL) {
			continue;
		} else {
			modnum_major++;
		}
	}
	max_index = modnum_major;

	for (major = 0; major < MAX_MODNUM; major++) {
		module = search_module_major(major);
		if (module == NULL) {
			continue;
		} else {
			module->index = index;
			init_queue[index] = *module;
			index++;
			store_depmodule(module);
		}
	}

	module_number = max_index+index_minor;

	for (i = 0; i < module_number; i++) {
		for (j = 0; j < init_queue[i].pcnt; j++) {
			init_queue[i].parents_index[j]
			    = init_queue[i].parents[j]->index;
		}
	}
}

static void
usage(char *cmd)
{
	char	*p = strrchr(cmd, '/');

	if (p != NULL) {
		cmd = p + 1;
	}

	(void)fprintf(stderr, "Usage: %s [-o outfile] -m name_to_major "
		      "dependfile\n", cmd);
	exit(1);
}

/*
 * int
 * main(int argc, char *argv[])
 *      The main function of creatheader.
 *
 * Calling/Exit State:
 *	Upon successful completion, main() returns zero.
 * 	Otherwise returns non-zero value.
 */
int
main(int argc, char **argv)
{
	int	rtn, c;
	modinfo_t *mod = NULL, *tmp = NULL;
	char	*majfile = NULL, *outfile = NULL;
	char	*cmd = *argv;

	while ((c = getopt(argc, argv, "o:m:")) != EOF) {
		switch (c) {
		case 'o':
			outfile = optarg;
			break;

		case 'm':
			majfile = optarg;
			break;

		default:
			usage(cmd);
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 1 || majfile == NULL) {
		usage(cmd);
		/* NOTREACHED */
	}

	head = NULL;
	tail = NULL;
	max_index = 0;
	module_number = 0;
	index_minor = 0;

	rtn = read_moduledependence(*argv);
	if (rtn == ERR_OPENFILE) {
		(void)fprintf(stderr, "Error : Cannot open module_depends.\n");
		goto error;
	} else if (rtn == ERR_FORMAT_SYMBOLE1 || rtn == ERR_FORMAT_SYMBOLE2) {
		(void)fprintf(stderr, "Error : Format error(%d)\n", -rtn);
		goto error;
	} else if (rtn == ERR_MEMALLOC) {
		(void)fprintf(stderr, "Error : Cannot allocate memory.\n");
		goto error;
	} else if(rtn == ERR_INTERNAL){
		(void)fprintf(stderr, "Error : Internal error(%d).\n", -rtn);
		goto error;
	}

	rtn = read_nametomajor(majfile);
	if (rtn == ERR_OPENFILE) {
		(void)fprintf(stderr, "Error : Cannot open name_to_major.\n");
		goto error;
	} else if (rtn == ERR_MEMALLOC) {
		(void)fprintf(stderr, "Error : Cannot allocate memory.\n");
		goto error;
	}

	create_modulelist();

	rtn = write_header(outfile);
	if (rtn == ERR_OPENFILE) {
		(void)fprintf(stderr, "Error : Cannot write to %s.\n",
			      (outfile == NULL) ? "<stdout>" : outfile);
		goto error;
	}

error:
	/* free module list */
	for (mod = head ; mod != NULL; mod = tmp ) {
		tmp = mod->next;
		free(mod);
	}

	rtn = ((rtn == 0) ? rtn : -rtn);
	return rtn;
}
