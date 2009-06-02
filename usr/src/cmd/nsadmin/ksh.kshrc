#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)ksh.kshrc	1.1	07/06/27 SMI"
#

#
# This file is sourced by interactive ksh93 shells before ${HOME}/.kshrc
#

# Enable "gmacs" editor mode if the user did not set an input mode yet
# (for example via ${EDITOR}, ${VISUAL} or any "set -o" flag)
if [[ "$(set +o)" != ~(E)--(gmacs|emacs|vi)( |$) ]] ; then
    set -o gmacs
fi


# enable multiline input mode
#set -o multiline

