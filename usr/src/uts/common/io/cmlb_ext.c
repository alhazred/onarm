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
 * Copyright (c) 2008 NEC Corporation
 */

#include <sys/scsi/scsi.h>
#include <sys/sunddi.h>
#include <sys/dklabel.h>
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/dktp/fdisk.h>
#include <sys/vtrace.h>
#include <sys/uuid.h>
#include <sys/cmlb.h>
#include <sys/cmlb_impl.h>

#ifdef DISK_ACCESS_CTRL
int cmlb_acc_ctrl_instance = -1;

static int cl_atoi(char **p);
void cmlb_access_ctrl_invalidate_nodes(struct cmlb_lun *cl, int instance,
    struct driver_minor_data *dmdp);
void cmlb_access_ctrl_invalidate_extpartition(struct cmlb_lun *cl,
    struct driver_minor_data dmdp_blk[], struct driver_minor_data dmdp_chr[]);
#endif

#ifdef CMLB_EXTPART
int cmlb_create_logical_partition_nodes(struct cmlb_lun *cl, int instance);
void cmlb_setup_logical_partition(struct cmlb_lun *cl);
int cmlb_read_logical_partition(struct cmlb_lun *cl,
    struct ipart fdisk[], caddr_t bufp, int *uidx, uint_t *solaris_offset,
    daddr_t *solaris_size, uint32_t blocksize, void *tg_cookie);
int cmlb_dkio_get_ebr(struct cmlb_lun *cl, caddr_t arg, int flag,
    void *tg_cookie);
int cmlb_dkio_set_ebr(struct cmlb_lun *cl, caddr_t arg, int flag,
    void *tg_cookie);


#define isExtended(pid)		\
	(((pid) == EXTDOS) || ((pid) == FDISK_EXTLBA))

#define isPartition(pid)	\
	(((pid) != EXTDOS) && ((pid) != FDISK_EXTLBA))

#define isSolarisPartition(pid)	\
	(((pid) == SUNIXOS) || ((pid) == SUNIXOS2))
#if defined(_LITTLE_ENDIAN)
#define ltohs(S)	(*((ushort_t *)(&(S))))
#define ltohi(I)	(*((uint_t *)(&(I))))
#else /* !_LITTLE_ENDIAN */
#define getbyte(A, N)	(((unsigned char *)(&(A)))[N])
#define ltohs(S)	((getbyte(S, 1) << 8) | getbyte(S, 0))
#define ltohi(I)	((getbyte(I, 3) << 24) | (getbyte(I, 2) << 16) | \
			    (getbyte(I, 1) << 8) | getbyte(I, 0))
#endif /* _LITTLE_ENDIAN */


/*
 *    Function: cmlb_create_logical_parititon_nodes
 *
 * Description: Create the minor device nodes of logical partition 
 *		for the instance.
 *
 *   Arguments:
 *	cl		driver soft state (unit) structure
 *	instance	device instance number
 *
 * Return Code: 0 success
 *		ENXIO	failure.
 *
 *     Context: Kernel thread context
 */
int
cmlb_create_logical_partition_nodes(struct cmlb_lun *cl, int instance)
{
	int i = 0;
	char name[48];

	while (i < _EXTFDISK_PARTITION + 1) {
		(void) sprintf(name,"l%d", i);
		if (ddi_create_minor_node(CMLB_DEVINFO(cl),
		    name, S_IFBLK, 
		    (instance << CMLBUNIT_SHIFT) | (FDISK_P4 + i + 1),
		    cl->cl_node_type, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(CMLB_DEVINFO(cl), NULL);
			return (ENXIO);
		}

		(void) sprintf(name,"l%d,raw", i);
		if (ddi_create_minor_node(CMLB_DEVINFO(cl),
		    name, S_IFCHR,
		    (instance << CMLBUNIT_SHIFT) | (FDISK_P4 + i + 1),
		    cl->cl_node_type, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(CMLB_DEVINFO(cl), NULL);
			return (ENXIO);
		}
		i++;
	}

	return (0);
}

/*
 *    Function: cmlb_setup_logical_partition
 *
 * Description: Set up the logical partitions,
 *		if we have valid geometry.
 *				
 *    Arguments:
 *	cl		driver soft state (unit) structure
 *
 *     Context: Kernel thread only (can sleep).
 */
void
cmlb_setup_logical_partition(struct cmlb_lun *cl)
{
	int count = 0;

	/*
	 *	Note that dkl_cylno is not used for the fdisk map entries,
	 *	so we set it to an entirely bogus value.
	 */
	for (count = 0; count < 1 + _EXTFDISK_PARTITION; count++) {
		cl->cl_map[FDISK_P4 + 1 + count].dkl_cylno = -1;
		cl->cl_map[FDISK_P4 + 1 + count].dkl_nblk = 
		    cl->cl_fmap[FD_NUMPART + count].fmap_nblk;

		cl->cl_offset[FDISK_P4 + 1 + count] =
		    cl->cl_fmap[FD_NUMPART + count].fmap_start;
	}
}


/*
 *    Function: cmlb_read_logical_partition
 *
 * Description: Read the fdisk table of logical partition.
 *
 *   Arguments:
 *	cl		driver soft state (unit) structure
 *	fdisk[]		fdisk table of basic partition
 *	bufp		pointer to allocated buffer for transfer
 *	uidx		active solaris partition id
 *	solaris_offset	offset to solaris part
 *	solaris_size	size of solaris partition
 *	blocksize	blocksize of target device
 *	tg_cookie	cookie from target driver to be passed back to target
 *			driver when we call back to it through tg_ops.
 *
 * Return Code: 0 for success (includes not reading for no_fdisk_present case
 *		errnos from tg_rw if failed to read the first block.
 *
 *     Context: Kernel thread only (can sleep).
 */
int
cmlb_read_logical_partition(struct cmlb_lun *cl, struct ipart fdisk[],
    caddr_t bufp, int *uidx, uint_t *solaris_offset, daddr_t *solaris_size,
    uint32_t blocksize, void *tg_cookie)
{
	struct ipart	*ebr_fdisk;
	struct extboot	*ebr_ptr;
	daddr_t		lastseek = 0;
	daddr_t		diskblk = 0;
	daddr_t		xstartsect;
	int		extendedPart = -1;
	int		xnumsect = -1;
	int		num_lpart = 0;
	int		rval = 0;
	int		i;
	int		new_uidx, new_solaris_offset, new_solaris_size;

	new_uidx = *uidx;
	new_solaris_offset = *solaris_offset;
	new_solaris_size = *solaris_size;

	for (i = 0; i < FD_NUMPART; i++) {
		if (extendedPart < 0 && isExtended(fdisk[i].systid)) {
			extendedPart = i;
			break;
		}
	}

	if (extendedPart >= 0) {
		diskblk = xstartsect = ltohi(fdisk[extendedPart].relsect);
		xnumsect = ltohi(fdisk[extendedPart].numsect);
		cl->cl_fmap[FD_NUMPART].fmap_start = diskblk;
		cl->cl_fmap[FD_NUMPART].fmap_nblk = xnumsect;
		do {
			if (diskblk == lastseek) {
				break;
			}
			mutex_exit(CMLB_MUTEX(cl));

			rval = DK_TG_READ(cl, bufp, diskblk, blocksize,
			    tg_cookie);

			mutex_enter(CMLB_MUTEX(cl));
			if (rval != 0) {
				cmlb_dbg(CMLB_ERROR,  cl,
				    "cmlb_read_logical_partition: "
				    "fdisk read err\n");
				bzero(&cl->cl_fmap[FD_NUMPART],
				    (sizeof (struct fmap)
				    * (1 + _EXTFDISK_PARTITION)));
				return (rval);
			}
			lastseek = diskblk;
			ebr_ptr = (struct extboot *)bufp;

			/* signature check */
			if (ltohs(ebr_ptr->signature) != MBB_MAGIC) {
				cmlb_dbg(CMLB_ERROR,  cl,
				    "cmlb_read_logical_partition: "
				    "no fdisk\n");
				bzero(&cl->cl_fmap[FD_NUMPART],
				    (sizeof (struct fmap)
				    * (1 + _EXTFDISK_PARTITION)));
				return (EINVAL);
			}

			ebr_fdisk = (struct ipart *)&ebr_ptr->parts[0];	

			if (isPartition(ebr_fdisk->systid)) {
				int relsect;
				int numsect;

				if (ebr_fdisk->numsect == 0) {
					cl->cl_fmap[num_lpart + FD_NUMPART + 1]
					    .fmap_start = 0;
					cl->cl_fmap[num_lpart + FD_NUMPART + 1]
					    .fmap_nblk  = 0;
					num_lpart++;
					goto NextExtPartCheck;
				}

				relsect = LE_32(ebr_fdisk->relsect) + diskblk;
				numsect = LE_32(ebr_fdisk->numsect);
				cl->cl_fmap[num_lpart + FD_NUMPART + 1]
				    .fmap_start = relsect;
				cl->cl_fmap[num_lpart + FD_NUMPART + 1]
				    .fmap_nblk = numsect;
				num_lpart++;

				if (isSolarisPartition(ebr_fdisk->systid)) {
					if ((new_uidx == -1) || 
					    (ebr_fdisk->bootid == ACTIVE)) {
						new_uidx = num_lpart - 1;
						new_solaris_offset = relsect;
						new_solaris_size = numsect;
					}
				}
			}
NextExtPartCheck:
			ebr_fdisk++;
			if (isExtended(ebr_fdisk->systid)) {
				diskblk = xstartsect +
				    ltohi(ebr_fdisk->relsect);
			} else {
				break;
			}
		} while (num_lpart < _EXTFDISK_PARTITION);
	}

	*uidx = new_uidx;
	*solaris_offset = new_solaris_offset;
	*solaris_size = new_solaris_size;

	return (rval);
}


/*
 *    Function: cmlb_dkio_get_ebr
 *
 * Description: This routine is the driver entry point for handling user
 *		requests to get the current device EBR (DKIOCGEBR).
 *
 *   Arguments: 
 *	cl		driver soft state (unit) structure
 *	arg 		pointer to user provided extboot structure specifying
 *			the current EBR.
 *	flag 		this argument is a pass through to ddi_copyxxx()
 *			directly from the mode argument of ioctl().
 *	tg_cookie 	cookie from target driver to be passed back to target
 *			driver when we call back to it through tg_ops.
 *
 * Return Code: 0
 *		EINVAL
 *		EFAULT
 *		ENXIO
 */
int
cmlb_dkio_get_ebr(struct cmlb_lun *cl, caddr_t arg, int flag, void *tg_cookie)
{
	struct mboot	*mbp;
	caddr_t		bufp;
	int		rval;
	uint32_t	blocksize;
	struct ipart	*fdisk;
	struct extboot	*ebr;
	struct extboot	*ebp;
	daddr_t		lastseek = 0;	/* Disk block we sought previously */
	daddr_t		diskblk = 0;	/* Disk block to get */
	daddr_t		xstartsect;	/* base of Extended DOS partition */
	int		xnumsect= -1;	/* length of extended DOS partition */
	int		i;
	int		partnum = 0;
	int		extendedPart = -1;

	if (arg == NULL) {
		return (EINVAL);
	}

	blocksize = sizeof (struct mboot);
	bufp = kmem_alloc(blocksize, KM_SLEEP);
	ebr = kmem_zalloc(sizeof(struct extboot) * (_EXTFDISK_PARTITION + 1),
	    KM_SLEEP);

	/* get mboot record */
	if ((rval = DK_TG_READ(cl, bufp, 0, blocksize, tg_cookie)) != 0) {
		goto end;
	}

	mbp = (struct mboot *)bufp;
	if (ltohs(mbp->signature) != MBB_MAGIC) {
		rval = EINVAL;
		goto end;
	}

	/* copy mboot record */
	bcopy(bufp, ebr, sizeof(struct extboot));
	partnum++;

	fdisk = (struct ipart *)&(mbp)->parts[0];
	/* get extended boot record */
	for (i = 0; i < FD_NUMPART; i++) {
		if (isExtended(fdisk->systid)) {
			extendedPart = i;
			break;
		}
		fdisk++;
	}

	if (extendedPart >= 0) {
		diskblk = xstartsect = ltohi(fdisk->relsect);
		xnumsect = ltohi(fdisk->numsect);

		do {
			if (diskblk == lastseek) {
				break;
			}

			if ((rval = DK_TG_READ(cl, bufp, diskblk, blocksize,
			    tg_cookie)) != 0) {
				goto end;
			}
			lastseek = diskblk;

			ebp = (struct extboot *)bufp;
			if (ltohs(ebp->signature) != MBB_MAGIC) {
				rval = EINVAL;
				goto end;
			}

			fdisk = (struct ipart *)&ebp->parts[0];
			fdisk++;
			bcopy(ebp, (ebr + partnum), sizeof(struct extboot));
			partnum++;

			if (isExtended(fdisk->systid)) {
				diskblk = xstartsect + ltohi(fdisk->relsect);
			} else {
				break;
			}
		} while(partnum < _EXTFDISK_PARTITION + 1);
		rval = 0;

		if (ddi_copyout(ebr, (void *)arg,
		    sizeof(struct extboot) * (_EXTFDISK_PARTITION + 1),
		    flag) != 0) {
			rval = EFAULT;
		}
	}

end:
	kmem_free(bufp, blocksize);
	kmem_free(ebr, sizeof(struct extboot) * (_EXTFDISK_PARTITION + 1));
	return (rval);
}

/*
 *    Function: cmlb_dkio_set_ebr
 *
 * Description: This routine is the driver entry point for handling user
 *		requests to validate and set the device EBR (DKIOCSEBR).
 *
 *   Arguments:
 *	cl		driver soft state (unit) structure
 *	arg		pointer to user provided extboot structure used
 *			to set the EBR.
 *	flag		this argument is a pass through to ddi_copyxxx()
 *			directly from the mode argument of ioctl().
 *	tg_cookie	cookie from target driver to be passed back to target
 *			driver when we call back to it through tg_ops.
 *
 * Return Code: 0
 *		EINVAL
 *		EFAULT
 *		ENXIO
 */
int
cmlb_dkio_set_ebr(struct cmlb_lun *cl, caddr_t arg, int flag, void *tg_cookie)
{
	int		rval;
	int		i;
	struct extboot	*ebr;
	struct ipart	*fdisk;
	daddr_t		diskblk = 0;
	daddr_t		xstartsect;
	int		xnumsect = -1;
	int		extendedPart = -1;
	int		extendedPartnum = -1;

#ifdef DISK_ACCESS_CTRL
	if ((strcmp(ddi_driver_name(CMLB_DEVINFO(cl)), "sd") == 0) &&
	    (ddi_get_instance(CMLB_DEVINFO(cl)) == cmlb_acc_ctrl_instance)) {
		return (ENOTTY);
	}
#endif
	ASSERT(!mutex_owned(CMLB_MUTEX(cl)));

	if (arg == NULL) {
		return (EINVAL);
	}

	ebr = kmem_alloc((sizeof(struct extboot) * (_EXTFDISK_PARTITION + 1)),
	    KM_SLEEP);

	if (ddi_copyin((const void *)arg, ebr,
	    (sizeof (struct extboot) * (_EXTFDISK_PARTITION + 1)), flag) != 0) {
		rval = EFAULT;
		goto end;
	}

	/* check mboot record signiture */
	if(ltohs(ebr->signature) != MBB_MAGIC) {
		rval = EINVAL;
		goto end;
	}

	/* check extended record signiture */
	for (i = 1; i < _EXTFDISK_PARTITION + 1; i++) {
		if(ltohs((ebr + i)->signature) != MBB_MAGIC) {
			rval = EINVAL;
			goto end;
		}
		fdisk = (struct ipart *)&(ebr + i)->parts[0];
		fdisk++;

		if (!isExtended(fdisk->systid)) {
			extendedPartnum = i;
			break;
		}
	}

	fdisk = (struct ipart *)&(ebr)->parts[0];
	/* check Extended Partitions */
	for (i = 0; i < FD_NUMPART; i++) {
		if (isExtended(fdisk->systid)) {
			extendedPart = i;
			break;
		}
		fdisk++;
	}

	if (extendedPart < 0) {
		rval = EINVAL;
		goto end;
	}

	diskblk = xstartsect = ltohi(fdisk->relsect);
	xnumsect = ltohi(fdisk->numsect);

	for (i = 1; i <= extendedPartnum; i++) {
		if ((rval = DK_TG_WRITE(cl, (ebr + i), diskblk,
		    cl->cl_sys_blocksize, tg_cookie)) != 0) {
			goto end;
		}
		fdisk = (struct ipart *)&(ebr + i)->parts[0];
		fdisk++;
		diskblk = xstartsect + ltohi(fdisk->relsect);
	}

	mutex_enter(CMLB_MUTEX(cl));
#if defined(__i386) || defined(__amd64) || defined(__arm)
	rval = cmlb_update_fdisk_and_vtoc(cl, tg_cookie);
	if ((cl->cl_f_geometry_is_valid == FALSE) || (rval != 0)) {
		mutex_exit(CMLB_MUTEX(cl));
		goto end;
	}

#ifdef __lock_lint
	cmlb_setup_default_geometry(cl, tg_cookie);
#endif

#else
	if (rval == 0) {
		if (cl->cl_blockcount <= DK_MAX_BLOCKS)
			cmlb_setup_default_geometry(cl, tg_cookie);
	}
#endif
	mutex_exit(CMLB_MUTEX(cl));
end:
	kmem_free(ebr,
	    (size_t)(sizeof(struct extboot) * (_EXTFDISK_PARTITION + 1)));
	return (rval);
}
#endif /* CMLB_EXTPART */

#ifdef DISK_ACCESS_CTRL
#define CMLB_SKIP_SPACE(x)	{		\
	while (*(x) == ' ' || *(x) =='\t') {	\
		(x)++;				\
	}					\
}

static int
cl_atoi(char **p)
{
	int cl_disgit = 0, rtn = 0;

	while ((**p >= '0') && (**p <= '9')) {
		cl_disgit = 1;
		rtn = (rtn * 10) + (**p - '0');
		(*p)++;
	}

	if (cl_disgit != 1) {
		rtn = -1;
	}
	return (rtn);
}

void
cmlb_access_ctrl_invalidate_nodes(struct cmlb_lun *cl, int instance,
		struct driver_minor_data *dmdp)
{
	char	*access = DISK_ACCESS_CONFIG;
	char	*access_list = NULL, *p;
	int	acc_inst = 0, acc_err = 0, i;
	char	name[48];

#define	_CMLB_ACC_BAD_INST	1
#define	_CMLB_ACC_BAD_FORM	2
#define	_CMLB_ACC_BAD_PRIM	3
#define	_CMLB_ACC_BAD_LOGI	4
#define	_CMLB_ACC_BAD_NDEF	5

	p = access;

	if (strcmp(ddi_driver_name(CMLB_DEVINFO(cl)), "sd") != 0) {
		return;
	}

	/* get instance */
	CMLB_SKIP_SPACE(p);
	acc_inst = cl_atoi(&p);
	if (acc_inst == -1) {
		acc_err = _CMLB_ACC_BAD_INST;
		goto config_end;
	}
	cmlb_acc_ctrl_instance = acc_inst;

	instance = ddi_get_instance(CMLB_DEVINFO(cl));
	if (acc_inst != instance) {
		goto end;
	}
	CMLB_SKIP_SPACE(p);

	if (*p != ',') {
		acc_err = _CMLB_ACC_BAD_FORM;
		goto config_end;
	}
	p++;

	access_list = kmem_zalloc((sizeof(char) * MAXPART), KM_SLEEP);

	/* get partition list */
	while (*p != '\0') {
		int	index = 0, num = 0;

		CMLB_SKIP_SPACE(p);

		switch (*p) {
		case 's':
			/* slice */
			p++;
			cl_atoi(&p);
			CMLB_SKIP_SPACE(p);
			if (*p == ',') {
				p++;
			}
			continue;
		case 'p':
			/* primary partition */
			index = NDKMAP;
			p++;
			num = cl_atoi(&p);
			if ((num < 0) || (num > FD_NUMPART)) {
				acc_err = _CMLB_ACC_BAD_PRIM;
				goto config_end;
			}
			break;
#ifdef CMLB_EXTPART
		case 'l':
			/* logical partition */
			index = FDISK_P4 + 1;
			p++;
			num = cl_atoi(&p);
			if ((num < 0) || (num > _EXTFDISK_PARTITION)) {
				acc_err = _CMLB_ACC_BAD_LOGI;
				goto config_end;
			}
			break;
#endif
		default:
			acc_err = _CMLB_ACC_BAD_NDEF;
			goto config_end;
		}

		index += num;	/* calc part number */
		access_list[index] = 1;

		CMLB_SKIP_SPACE(p);

		if (*p++ != ',') {
			break;
		}
	}
config_end:
	if (acc_err != 0) {
		cmn_err(CE_WARN, "cmlb_access_ctrl_invalidate_nodes: "
			"invalid character(%s)", access);
		cmlb_dbg(CMLB_TRACE, cl,"cmlb_access_ctrl_invalidate_nodes:"
			" error type = %d\n", acc_err);
		goto end;
	}

	while (dmdp->name != NULL) {
		if (access_list[dmdp->minor] == 1) {
			/* remove minor node */
			ddi_remove_minor_node(CMLB_DEVINFO(cl), dmdp->name);
		}
		dmdp++;
	}

#ifdef CMLB_EXTPART
	ddi_remove_minor_node(CMLB_DEVINFO(cl), "l0");
	ddi_remove_minor_node(CMLB_DEVINFO(cl), "l0,raw");
	for (i = 1; i < (_EXTFDISK_PARTITION + 1); i++) {
		if (access_list[i + FDISK_P4 + 1] == 1) {
			(void) sprintf(name,"l%d", i);
			ddi_remove_minor_node(CMLB_DEVINFO(cl), name);

			(void) sprintf(name,"l%d,raw", i);
			ddi_remove_minor_node(CMLB_DEVINFO(cl), name);
		}
	}
#endif
end:
	if (access_list != NULL) {
		kmem_free(access_list, (sizeof(char) * MAXPART));
	}
#undef	_CMLB_ACC_BAD_INST
#undef	_CMLB_ACC_BAD_FORM
#undef	_CMLB_ACC_BAD_PRIM
#undef	_CMLB_ACC_BAD_LOGI
#undef	_CMLB_ACC_BAD_NDEF
}

void
cmlb_access_ctrl_invalidate_extpartition(
	struct cmlb_lun *cl,
	struct driver_minor_data dmdp_blk[],
	struct driver_minor_data dmdp_chr[])
{
	if ((strcmp(ddi_driver_name(CMLB_DEVINFO(cl)), "sd") != 0) ||
	    (ddi_get_instance(CMLB_DEVINFO(cl)) != cmlb_acc_ctrl_instance)) {
		return;
	}

	if (cl->cl_map[FDISK_P4 + 1].dkl_nblk != 0) {
		/* Extended partition is defined */
		int i;
		for (i = FDISK_P1; i <= FDISK_P4; i++) {
			if (cl->cl_offset[FDISK_P4 + 1] == cl->cl_offset[i]) {
				cmlb_dbg(CMLB_TRACE, cl,
				    "cmlb_access_ctrl_invalidate_extpartition "
				    ":: remove[%s]\n",
				    dmdp_blk[i - FDISK_P1].name);
				ddi_remove_minor_node(CMLB_DEVINFO(cl),
					dmdp_blk[i - FDISK_P1].name);
				ddi_remove_minor_node(CMLB_DEVINFO(cl),
					dmdp_chr[i - FDISK_P1].name);
				break;
			}
		}
	}
}
#endif /* DISK_ACCESS_CTRL */
