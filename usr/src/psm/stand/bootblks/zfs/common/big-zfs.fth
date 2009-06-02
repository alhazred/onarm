

\ ident	"@(#)big-zfs.fth	1.1	07/11/29 SMI"
\ Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
\ Use is subject to license terms.
\
\ CDDL HEADER START
\
\ The contents of this file are subject to the terms of the
\ Common Development and Distribution License (the "License").
\ You may not use this file except in compliance with the License.
\
\ You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
\ or http://www.opensolaris.org/os/licensing.
\ See the License for the specific language governing permissions
\ and limitations under the License.
\
\ When distributing Covered Code, include this CDDL HEADER in each
\ file and include the License file at usr/src/OPENSOLARIS.LICENSE.
\ If applicable, add the following below this CDDL HEADER, with the
\ fields enclosed by brackets "[]" replaced with your own identifying
\ information: Portions Copyright [yyyy] [name of copyright owner]
\
\ CDDL HEADER END
\


id: @(#)big-zfs.fth	1.1	07/11/29 SMI
purpose: ZFS debug fs reader
copyright: Copyright 2007 Sun Microsystems, Inc. All Rights Reserved

\ add headers
create doheaders
create bigbootblk

: fs-pkg$   " zfs-file-system"  ;
: fs-type$  " zfs"  ;

\ load common words
fload ../../../common/util.fth

\ load fs reader
fload ../../common/zfs.fth