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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * ident	"@(#)DropEvent.java	1.3	07/02/13 SMI"
 */
package org.opensolaris.os.dtrace;

import java.io.*;
import java.util.EventObject;

/**
 * Notification that DTrace has dropped data due to inadequate buffer
 * space.
 *
 * @see ConsumerListener#dataDropped(DropEvent e)
 *
 * @author Tom Erickson
 */
public class DropEvent extends EventObject {
    static final long serialVersionUID = 5454623535426339134L;

    /** @serial */
    private Drop drop;

    /**
     * Creates a {@link ConsumerListener#dataDropped(DropEvent e)
     * dataDropped()} event that reports dropped data.
     *
     * @throws NullPointerException if the given drop is {@code null}
     */
    public
    DropEvent(Object source, Drop dataDrop)
    {
	super(source);
	drop = dataDrop;
	validate();
    }

    private final void
    validate()
    {
	if (drop == null) {
	    throw new NullPointerException("drop is null");
	}
    }

    /**
     * Gets the drop information generated by DTrace.
     *
     * @return non-null drop information generated by DTrace
     */
    public Drop
    getDrop()
    {
	return drop;
    }

    private void
    readObject(ObjectInputStream s)
            throws IOException, ClassNotFoundException
    {
	s.defaultReadObject();
	// check invariants
	try {
	    validate();
	} catch (Exception e) {
	    InvalidObjectException x = new InvalidObjectException(
		    e.getMessage());
	    x.initCause(e);
	    throw x;
	}
    }

    /**
     * Gets a string representation of this event useful for logging and
     * not intended for display.  The exact details of the
     * representation are unspecified and subject to change, but the
     * following format may be regarded as typical:
     * <pre><code>
     * class-name[property1 = value1, property2 = value2]
     * </code></pre>
     */
    public String
    toString()
    {
	StringBuilder buf = new StringBuilder();
	buf.append(DropEvent.class.getName());
	buf.append("[source = ");
	buf.append(getSource());
	buf.append(", drop = ");
	buf.append(drop);
	buf.append(']');
	return buf.toString();
    }
}
