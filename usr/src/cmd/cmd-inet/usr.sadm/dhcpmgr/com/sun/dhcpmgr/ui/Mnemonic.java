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
 * ident	"%Z%%M%	%I%	%E% SMI"
 *
 * Copyright 2002 by Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
package com.sun.dhcpmgr.ui;

import java.util.*;


public class Mnemonic {

    public String  string = "";
    public int     mnemonic = 0;

    public Mnemonic(String locStr) {
        string = locStr;

        int i = string.indexOf('&');
        if (i > -1) {
            String sUpper = string.toUpperCase();
            mnemonic = sUpper.charAt(i+1);
	    StringBuffer s = new StringBuffer(string);
	    s.delete(i, i+1);
	    string = s.toString();
        }
    }

    public String getString() {

        return string;

    } // getString

    public int getMnemonic() {

        return mnemonic;

    } // getMnemonic
}
