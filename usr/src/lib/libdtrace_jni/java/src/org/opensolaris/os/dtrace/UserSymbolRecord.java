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
 * ident	"@(#)UserSymbolRecord.java	1.3	07/02/13 SMI"
 */
package org.opensolaris.os.dtrace;

import java.io.*;
import java.beans.*;

/**
 * A value generated by the DTrace {@code umod()}, {@code ufunc()}, or
 * {@code usym()} action used to lookup the symbol associated with a
 * user address.
 * <p>
 * Immutable.  Supports persistence using {@link java.beans.XMLEncoder}.
 *
 * @author Tom Erickson
 */
public final class UserSymbolRecord implements SymbolValueRecord,
       Serializable, Comparable <UserSymbolRecord>
{
    static final long serialVersionUID = -591954442654439794L;

    static {
	try {
	    BeanInfo info = Introspector.getBeanInfo(UserSymbolRecord.class);
	    PersistenceDelegate persistenceDelegate =
		    new DefaultPersistenceDelegate(
		    new String[] {"processID", "symbol", "address"})
	    {
		/*
		 * Need to prevent DefaultPersistenceDelegate from using
		 * overridden equals() method, resulting in a
		 * StackOverFlowError.  Revert to PersistenceDelegate
		 * implementation.  See
		 * http://forum.java.sun.com/thread.jspa?threadID=
		 * 477019&tstart=135
		 */
		protected boolean
		mutatesTo(Object oldInstance, Object newInstance)
		{
		    return (newInstance != null && oldInstance != null &&
			    oldInstance.getClass() == newInstance.getClass());
		}
	    };
	    BeanDescriptor d = info.getBeanDescriptor();
	    d.setValue("persistenceDelegate", persistenceDelegate);
	} catch (IntrospectionException e) {
	    e.printStackTrace();
	}
    }

    // serialized explicitly to hide implementation; treat as final
    private transient Value value;
    // set natively after creation; treat as final
    private transient String symbol;

    /**
     * Called by native code.
     */
    private
    UserSymbolRecord(int pid, long addressValue)
    {
	value = new Value(pid, addressValue);
    }

    /**
     * Creates a {@code UserSymbolRecord} with the given process ID,
     * symbol lookup, and user address converted in probe context as a
     * result of the DTrace {@code umod()}, {@code ufunc()}, or {@code
     * usym()} action.
     * <p>
     * Supports XML persistence.
     *
     * @param pid non-negative user process ID
     * @param lookupValue the result in the native DTrace library of
     * looking up the symbol associated with the given user address
     * @param addressValue symbol address
     * @throws NullPointerException if the given lookup value is {@code null}
     * @throws IllegalArgumentException if the given process ID is
     * negative
     */
    public
    UserSymbolRecord(int pid, String lookupValue, long addressValue)
    {
	value = new Value(pid, addressValue);
	symbol = lookupValue;
	validate();
    }

    private final void
    validate()
    {
	if (symbol == null) {
	    throw new NullPointerException("symbol is null");
	}
    }

    /**
     * Gets the process ID associated with this record's symbol.
     *
     * @return non-negative pid
     */
    public int
    getProcessID()
    {
	return value.getProcessID();
    }

    /**
     * Gets the result of the address lookup in the same form returned
     * by {@link Consumer#lookupUserFunction(int pid, long address)}.
     *
     * @return non-null address lookup in the format defined by the
     * native DTrace library
     */
    public String
    getSymbol()
    {
	return symbol;
    }

    /**
     * Called by native code and ProbeData addSymbolRecord()
     */
    void
    setSymbol(String lookupValue)
    {
	symbol = lookupValue;
	validate();
    }

    /**
     * Gets the symbol's user address.
     *
     * @return the symbol's user address
     */
    public long
    getAddress()
    {
	return value.getAddress();
    }

    /**
     * Gets the composite value of the symbol's process ID and address.
     * The value is used in {@link #equals(Object o) equals()} and
     * {@link #compareTo(UserSymbolRecord r) compareTo()} to test
     * equality and to determine the natural ordering of {@code
     * UserSymbolRecord} instances.
     *
     * @return non-null composite value of the symbols's process ID and
     * address
     */
    public Value
    getValue()
    {
	return value;
    }

    /**
     * Compares the specified object with this {@code UserSymbolRecord}
     * for equality.  Returns {@code true} if and only if the specified
     * object is also a {@code UserSymbolRecord} and both records have
     * the same process ID and the same address.
     *
     * @return {@code true} if and only if the specified object is also
     * a {@code UserSymbolRecord} and both records have the same
     * process ID and the same address
     */
    @Override
    public boolean
    equals(Object o)
    {
	if (o instanceof UserSymbolRecord) {
	    UserSymbolRecord r = (UserSymbolRecord)o;
	    return value.equals(r.value);
	}
	return false;
    }

    /**
     * Overridden to ensure that equal instances have equal hash codes.
     */
    @Override
    public int
    hashCode()
    {
	return value.hashCode();
    }

    /**
     * Compares this record with the given user symbol lookup and orders
     * by the combined value of process ID first and address second.
     * The comparison treats addresses as unsigned values so the
     * ordering is consistent with that defined in the native DTrace
     * library. The {@code compareTo()} method is compatible with {@link
     * #equals(Object o) equals()}.
     *
     * @return -1, 0, or 1 as this record's combined process ID and
     * address is less than, equal to, or greater than the given
     * record's combined process ID and address
     */
    public int
    compareTo(UserSymbolRecord r)
    {
	return value.compareTo(r.value);
    }

    /**
     * Serialize this {@code UserSymbolRecord} instance.
     *
     * @serialData processID ({@code int}), followed by symbol ({@code
     * java.lang.String}), followed by address ({@code long})
     */
    private void
    writeObject(ObjectOutputStream s) throws IOException
    {
	s.defaultWriteObject();
	s.writeInt(getProcessID());
	s.writeObject(getSymbol());
	s.writeLong(getAddress());
    }

    private void
    readObject(ObjectInputStream s)
            throws IOException, ClassNotFoundException
    {
	s.defaultReadObject();
	int pid = s.readInt();
	symbol = (String)s.readObject();
	long addressValue = s.readLong();
	try {
	    value = new Value(pid, addressValue);
	    validate();
	} catch (Exception e) {
	    InvalidObjectException x = new InvalidObjectException(
		    e.getMessage());
	    x.initCause(e);
	    throw x;
	}
    }

    /**
     * Gets the result of this symbol lookup.  The format is defined in
     * the native DTrace library and is as stable as that library
     * definition.
     *
     * @return {@link #getSymbol()}
     */
    @Override
    public String
    toString()
    {
	return symbol;
    }

    /**
     * The composite value of a symbol's process ID and user address.
     * <p>
     * Immutable.  Supports persistence using {@link
     * java.beans.XMLEncoder}.
     */
    public static final class Value implements Serializable,
	    Comparable <Value> {
	static final long serialVersionUID = -91449037747641135L;

	static {
	    try {
		BeanInfo info = Introspector.getBeanInfo(
			UserSymbolRecord.Value.class);
		PersistenceDelegate persistenceDelegate =
			new DefaultPersistenceDelegate(
			new String[] {"processID", "address"})
		{
		    /*
		     * Need to prevent DefaultPersistenceDelegate from using
		     * overridden equals() method, resulting in a
		     * StackOverFlowError.  Revert to PersistenceDelegate
		     * implementation.  See
		     * http://forum.java.sun.com/thread.jspa?threadID=
		     * 477019&tstart=135
		     */
		    protected boolean
		    mutatesTo(Object oldInstance, Object newInstance)
		    {
			return (newInstance != null && oldInstance != null &&
				oldInstance.getClass() ==
				newInstance.getClass());
		    }
		};
		BeanDescriptor d = info.getBeanDescriptor();
		d.setValue("persistenceDelegate", persistenceDelegate);
	    } catch (IntrospectionException e) {
		e.printStackTrace();
	    }
	}

	/** @serial */
	private final int processID;
	/** @serial */
	private final long address;

	/**
	 * Creates a composite value with the given user process ID and
	 * symbol address.
	 * <p>
	 * Supports XML persistence.
	 *
	 * @param pid non-negative process ID
	 * @param addressValue symbol address
	 * @throws IllegalArgumentException if the given process ID is
	 * negative
	 */
	public
	Value(int pid, long addressValue)
	{
	    processID = pid;
	    address = addressValue;
	    validate();
	}

	private final void
	validate()
	{
	    if (processID < 0) {
		throw new IllegalArgumentException("process ID is negative");
	    }
	}

	/**
	 * Gets the process ID associated with this value's user
	 * address.
	 *
	 * @return non-negative pid
	 */
	public int
	getProcessID()
	{
	    return processID;
	}

	/**
	 * Gets the symbol's user address.
	 *
	 * @return the symbol's user address
	 */
	public long
	getAddress()
	{
	    return address;
	}

	/**
	 * Compares the specified object with this {@code
	 * UserSymbolRecord.Value} for equality.  Returns {@code true}
	 * if and only if the specified object is also a {@code
	 * UserSymbolRecord.Value} and both values have the same process
	 * ID and the same address.
	 *
	 * @return {@code true} if and only if the specified object is
	 * also a {@code UserSymbolRecord.Value} and both values have
	 * the same process ID and the same address
	 */
	@Override
	public boolean
	equals(Object o)
	{
	    if (o instanceof Value) {
		Value v = (Value)o;
		return ((processID == v.processID) && (address == v.address));
	    }
	    return false;
	}

	/**
	 * Overridden to ensure that equal instances have equal hash
	 * codes.
	 */
	@Override
	public int
	hashCode()
	{
	    int hash = 17;
	    hash = 37 * hash + processID;
	    hash = 37 * hash + (int)(address ^ (address >>> 32));
	    return hash;
	}

	/**
	 * Compares this value with the given {@code
	 * UserSymbolRecord.Value} and orders by process ID first and
	 * address second.  The comparison treats addresses as unsigned
	 * values so the ordering is consistent with that defined in the
	 * native DTrace library. The {@code compareTo()} method is
	 * compatible with {@link #equals(Object o) equals()}.
	 *
	 * @return -1, 0, or 1 as this value's combined process ID and
	 * address is less than, equal to, or greater than the given
	 * value's combined process ID and address
	 */
	public int
	compareTo(Value v)
	{
	    int cmp;
	    cmp = (processID < v.processID ? -1 :
		    (processID > v.processID ? 1 : 0));
	    if (cmp == 0) {
		cmp = ProbeData.compareUnsigned(address, v.address);
	    }
	    return cmp;
	}

	private void
	readObject(ObjectInputStream s)
		throws IOException, ClassNotFoundException
	{
	    s.defaultReadObject();
	    // check class invariants
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
	 * Gets a string representation of this {@code
	 * UserSymbolRecord.Value} instance useful for logging and not
	 * intended for display.  The exact details of the
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
	    buf.append(Value.class.getName());
	    buf.append("[processID = ");
	    buf.append(processID);
	    buf.append(", address = ");
	    buf.append(address);
	    buf.append(']');
	    return buf.toString();
	}
    }
}
