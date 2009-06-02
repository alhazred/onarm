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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_IB_MGT_IBMF_IBMF_SAA_H
#define	_SYS_IB_MGT_IBMF_IBMF_SAA_H

#pragma ident	"@(#)ibmf_saa.h	1.2	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/ib/ib_types.h>
#include <sys/ib/mgt/sa_recs.h>

/*
 * SA Access Interface: Interfaces to enable access to the SA
 */

#define	IBMF_SAA_PKEY_WC	0	/* partition key wildcard */
#define	IBMF_SAA_MTU_WC		0	/* mtu wilcard for gid_to_pathrecords */

typedef enum _ibmf_saa_access_type_t {
	IBMF_SAA_RETRIEVE,
	IBMF_SAA_UPDATE,
	IBMF_SAA_DELETE
} ibmf_saa_access_type_t;

/*
 * ibmf_saa_handle
 *	Opaque handle to identify the consumer
 */
typedef struct ibmf_saa_handle *ibmf_saa_handle_t;

/*
 * ibmf_saa_cb_t
 * ibmf_saa's callback to clients to inform them that the response to an
 * asynchronous request has arrived or that the request timed out.
 *
 * Input Arguments
 * clnt_private - opaque handle to client specific data (sq_callback_arg)
 * length - size of response returned
 * result - pointer to buffer of response.  Data will be in host-endian format
 * and unpacked.  Client can just cast to a pointer to the structure
 * status - ibmf status.  Status can be any of the values returned by a
 * synchronous ibmf_sa_access() call.
 *
 * Output Arguments
 * none
 *
 * Returns
 * none
 */
typedef void (*ibmf_saa_cb_t) (
    void 	*callback_arg,
    size_t	length,
    char	*result,
    int		status);

/*
 * structure to provide parameters to ibmf_sa_access call
 */
typedef struct ibmf_saa_access_args_s {
	/* MAD attribute ID */
	uint16_t		sq_attr_id;

	/* retrieve, update, or delete */
	ibmf_saa_access_type_t 	sq_access_type;

	/* SA MAD component mask indicating fields in template to query on */
	uint64_t		sq_component_mask;

	/* pointer to template */
	void			*sq_template;

	/*
	 * length, in bytes, of template size for attributes which ibmf does
	 * not know about; ignored for known attribute id's.  length should be
	 * wire length and template for unknown attributes should be in wire
	 * format as ibmf will not be able to pack data.
	 */
	size_t			sq_template_length;

	/* callback and argument when asynchronous request returns */
	ibmf_saa_cb_t		sq_callback;
	void			*sq_callback_arg;
} ibmf_saa_access_args_t;

/*
 * enumeration of subnet events
 *
 * IBMF_SAA_EVENT_GID_AVAILABLE
 *              the identified gid is available
 * IBMF_SAA_EVENT_GID_UNAVAILABLE
 *              the identified gid is unavailable
 * IBMF_SAA_EVENT_MCG_CREATED
 *              MC group identified by mgid is created
 * IBMF_SAA_EVENT_MCG_DELETED
 *              MC group identified by mgid is deleted
 * IBMF_SAA_EVENT_CAP_MASK_CHG
 *              Portinfo.CapabilityMask changed
 * IBMF_SAA_EVENT_SYS_IMG_GUID_CHG
 *              System Image GUID changed
 * IBMF_SAA_EVENT_SUBSCRIBER_STATUS_CHG
 *		Status of ibmf subscriptions changed
 */
typedef enum ibmf_saa_subnet_event_e {

	IBMF_SAA_EVENT_GID_AVAILABLE,
	IBMF_SAA_EVENT_GID_UNAVAILABLE,
	IBMF_SAA_EVENT_MCG_CREATED,
	IBMF_SAA_EVENT_MCG_DELETED,
	IBMF_SAA_EVENT_CAP_MASK_CHG,
	IBMF_SAA_EVENT_SYS_IMG_GUID_CHG,
	IBMF_SAA_EVENT_SUBSCRIBER_STATUS_CHG

} ibmf_saa_subnet_event_t;

/*
 * ibmf must subscribe with the Subnet Administrator to provide the subnet
 * events for its clients.  It registers for the four trap producer types: CA,
 * switch, router, and subnet management.  If any of these registrations fails
 * the ibmf will notify each client that registered for events.  Clients are
 * notified by ibmf through their registered callback and the
 * SUBSCRIBER_STATUS_CHG event.
 *
 * For this event, the event_details producer_type_status_mask will be set.
 * Each bit in the mask corresponds to a different producer type.  When the bit
 * is on the ibmf was able to successfully subscribe for events from that
 * producer.  When the bit is off, ibmf was unable to subscribe and clients may
 * not receive events from that producer type.
 *
 * For example, if the status_mask is 0xb then events will be received that
 * correspond to CA's, switches, and subnet management traps.  However, traps
 * generated by routers may not be received.
 *
 * The ibmf re-registers for events when the port transitions to active.  If the
 * event status mask changes the ibmf will generate another
 * SUBSCRIBER_STATUS_CHG event with the new producer type status mask.  When
 * clients register they should only expect to receive a SUBSCRIBER_STATUS_CHG
 * event if one of the registrations failed.  If all four registrations
 * succeeded no event will be generated.
 *
 * If the port goes down, a SUBSCRIBER_STATUS_CHG event is not generated.
 * Clients should realize that events will not be forwarded.  If the port
 * transitions back to active ibmf_saa will resubscribe on behalf of the client.
 * If this subscription fails a SUBSCRIBER_STATUS_CHG event will be generated.
 *
 */

#define	IBMF_SAA_EVENT_STATUS_MASK_PRODUCER_CA		(1 << 0)
#define	IBMF_SAA_EVENT_STATUS_MASK_PRODUCER_SWITCH	(1 << 1)
#define	IBMF_SAA_EVENT_STATUS_MASK_PRODUCER_ROUTER	(1 << 2)
#define	IBMF_SAA_EVENT_STATUS_MASK_PRODUCER_SM		(1 << 3)

/*
 * structure passed as event_details argument of ibmf_saa subnet event
 * callback.
 *
 * Only some of the structure members are valid for a given event as given
 * below:
 *
 * member              		event type
 * ------              		----------
 * ie_gid               	IBMF_SAA_EVENT_GID_AVAILABLE,
 *					IBMF_SAA_EVENT_GID_UNAVAILABLE,
 *                              	IBMF_SAA_EVENT_MCG_CREATED, and
 *                              	IBMF_SAA_EVENT_MCG_DELETED
 * ie_lid               	IBMF_SAA_EVENT_CAP_MASK_CHG and
 *                              	IBMF_SAA_EVENT_SYS_IMG_GUID_CHG
 * ie_capability_mask   	IBMF_SAA_EVENT_CAP_MASK_CHG
 * ie_sysimg_guid       	IBMF_SAA_EVENT_SYS_IMG_GUID_CHG
 * ie_producer_type_status_mask	IBMF_SAA_EVENT_SUBSCRIBER_STATUS_CHG
 *
 */
typedef struct ibmf_saa_event_details_s {
	ib_gid_t	ie_gid;
	ib_guid_t	ie_sysimg_guid;
	uint32_t	ie_capability_mask; /* values defined in sm_attr.h */
	ib_lid_t	ie_lid;
	uint8_t		ie_producer_event_status_mask;
} ibmf_saa_event_details_t;

/*
 * Callback invoked when one of the events the client subscribed for
 * at ibmf_sa_session_open() time happens.
 *
 * This callback can occur before ibmf_sa_session_open() returns.
 *
 * Each callback is on a separate thread.  ibmf clients may block in the event
 * callback.  However, under heavy system load ibmf may not be able to generate
 * event callbacks.  Also, event callbacks, including SUBSCRIBER_STATUS_CHG,
 * could be dispatched out-of-order.
 *
 * Arguments:
 * ibmf_saa_handle              - Client's ibmf_saa_handle
 * ibmf_saa_event               - event that caused the callback
 * event_details                - additional data for the event
 * callback_arg                 - event_callback_arg member of
 *                                ibmf_saa_subnet_event_args_t
 */
typedef void (*ibmf_saa_subnet_event_cb_t)(
	ibmf_saa_handle_t		ibmf_saa_handle,
	ibmf_saa_subnet_event_t		ibmf_saa_event,
	ibmf_saa_event_details_t	*event_details,
	void				*callback_arg);

typedef struct ibmf_saa_subnet_event_args_s {

	/* func to be called when a subnet event happens */
	ibmf_saa_subnet_event_cb_t	is_event_callback;

	/* call back arg */
	void				*is_event_callback_arg;

} ibmf_saa_subnet_event_args_t;

/*
 * ibmf_sa_session_open():
 *
 * Before using the ibmf_saa interface, consumers should register with the
 * ibmf_saa interface by calling ibmf_sa_session_open(). Upon a successful
 * registration, a handle is returned for use in subsequent interaction with the
 * ibmf_saa interface; this handle is also provided as an argument to subnet
 * event notification function.
 *
 * Consumers can register to be notified of subnet events such as GID
 * being available/unavailable.  Clients which provide a non-NULL event args
 * structure will have the is_event_callback function called when an event is
 * received or there is a failure in subscribing for events.  This callback may
 * be generated before the ibmf_sa_session_open() call returns.
 *
 * This interface blocks allocating memory, but not waiting for any packet
 * responses.
 *
 * Arguments:
 * port_guid            - GUID of the port.
 * event_args		- subnet event registration details
 * sm_key               - only filled in if the consumer is an SM
 * ibmf_version         - version of the interface (IBMF_VERSION)
 * flags                - unused
 *
 * Return values:
 * IBMF_SUCCESS         - registration succeeded
 * IBMF_BAD_PORT	- registration failed; active port not found
 * IBMF_BAD_PORT_STATE  - registration failed; port found but not active or
 * 			previous registration failed
 * IBMF_NO_MEMORY	- registration failed; could not allocate memory
 * IBMF_NO_RESOURCES    - registration failed due to a resource issue
 * IBMF_BUSY            - registration failed; too many clients registered
 *                      for this port
 * IBMF_TRANSPORT_FAILURE - failure with underlying transport framework
 * IBMF_INVALID_ARG     - ibmf_saa_handle arg was NULL
 */
int ibmf_sa_session_open(
		ib_guid_t			port_guid,
		ib_smkey_t			sm_key,
		ibmf_saa_subnet_event_args_t	*event_args,
		uint_t				ibmf_version,
		uint_t				flags,
		ibmf_saa_handle_t		*ibmf_saa_handle);

/*
 * ibmf_sa_session_close()
 *
 * Unregister a consumer of the SA_Access interface
 *
 * This interface blocks.
 *
 * Arguments:
 * ibmf_saa_handle	- handle returned from sa_session_open()
 * flags		- unused
 *
 * Return values:
 * IBMF_SUCCESS		- unregistration succeeded
 * IBMF_BAD_HANDLE	- unregistration failed; handle is not valid or
 *			  session_close has already been called
 * IBMF_INVALID_ARG	- ibmf_saa_handle arg was NULL
 *
 * All outstanding callbacks will be canceled before this function returns.
 */
int	ibmf_sa_session_close(
		ibmf_saa_handle_t	*ibmf_saa_handle,
		uint_t			flags);

/*
 * ibmf_sa_access
 *
 * Retrieve records from the SA given an AttributeID, ComponentMask,
 * and a template
 *
 * This interface blocks if the callback parameter is NULL.
 *
 * Input Arguments:
 * ibmf_saa_handle	- handle returned from ibmf_sa_session_open()
 * access_args 		- structure containing various parameters for the query
 * flags 		- unsused
 *
 * Output Arguments:
 * length		- size of buffer returned
 * result		- pointer to buffer of records returned in response.
 *			  Buffer is host-endian, unpacked can be cast to one of
 *			  the record types in sa_recs.h
 *
 * Return values:
 * IBMF_SUCCESS 	- query succeeded
 * IBMF_BAD_HANDLE	- sa session handle is invalid
 * IBMF_BAD_PORT_STATE	- port in incorrect state
 * IBMF_INVALID_ARG	- one of the pointer parameters was NULL
 * IBMF_NO_RESOURCES	- ibmf could not allocate ib resources or SA returned
 *			  ERR_NO_RESOURCES
 * IBMF_TRANS_TIMEOUT	- transaction timed out
 * IBMF_TRANS_FAILURE	- transaction failure
 * IBMF_NO_MEMORY	- ibmf could not allocate memory
 * IBMF_REQ_INVALID	- send and recv buffer the same for a sequenced
 *			  transaction or the SA returned an ERR_REQ_INVALID
 * IBMF_NO_RECORDS	- no records matched query
 * IBMF_TOO_MANY_RECORDS- SA returned SA_ERR_TOO_MANY_RECORDS
 * IBMF_INVALID_GID	- SA returned SA_INVALID_GID
 * IBMF_INSUFF_COMPS	- SA returned SA_ERR_INSUFFICIENT_COMPS
 * IBMF_UNSUPP_METHOD	- SA returned MAD_STATUS_UNSUPP_METHOD
 * IBMF_UNSUPP_METHOD_ATTR - SA returned MAD_STATUS_UNSUPP_METHOD_ATTR
 * IBMF_INVALID_FIELD	- SA returned MAD_STATUS_INVALID_FIELD
 * IBMF_NO_ACTIVE_PORTS - no active ports found
 *
 * Upon successful completion, result points to a buffer containing the records.
 * length is the size in bytes of the buffer returned in result.  If there are
 * no records or the call failed the length is 0.
 *
 * The consumer is responsible for freeing the memory associated with result.
 */
int	ibmf_sa_access(
		ibmf_saa_handle_t	ibmf_saa_handle,
		ibmf_saa_access_args_t	*access_args,
		uint_t			flags,
		size_t			*length,
		void			**result);

/*
 * Helper Functions.
 *	Ease of use functions so that the consumer doesn't
 * 	have to do the overhead of calling ibmf_sa_access() for
 *	commonly used queries
 */

/*
 * ibmf_saa_gid_to_pathrecords
 * 	Given a source gid and a destination gid, return paths
 *	between the gids.
 *
 * This interface blocks.
 *
 * Input Arguments:
 * ibmf_saa_handle	- handle returned from ibmf_sa_session_open()
 * sgid 		- source gid of path
 * dgid			- destination gid of path
 * p_key		- partition of path.  This value may be wildcarded with
 *			  IBMF_SAA_PKEY_WC.
 * mtu 			- preferred MTU of the path.  This argument may be
 *			  wildcarded with IBMF_SAA_MTU_WC.
 * reversible		- if B_TRUE, ibmf will query only reversible paths
 *			  see Infiniband Specification table 171
 * num_paths		- maximum number of paths to return
 *			  numpaths should be checked for the actual number of
 *			  records returned.
 * flags		- unused
 *
 * Output Arguments:
 * num_paths		- actual number of paths returned
 * length		- size of buffer returned
 * result		- pointer to buffer of path records returned in response
 *
 * Return values:
 * Error codes are the same as ibmf_sa_access() return values
 *
 * Upon successful completion, result points to a buffer containing the records.
 * length is the size in bytes of the buffer returned in result.  If there are
 * no records or the call failed the length is 0.
 *
 * The consumer is responsible for freeing the memory associated with result.
 */
int	ibmf_saa_gid_to_pathrecords(
		ibmf_saa_handle_t	ibmf_saa_handle,
		ib_gid_t		sgid,
		ib_gid_t		dgid,
		ib_pkey_t		p_key,
		ib_mtu_t		mtu,
		boolean_t		reversible,
		uint8_t			*num_paths,
		uint_t			flags,
		size_t			*length,
		sa_path_record_t	**result);
/*
 * ibmf_saa_paths_from_gid
 *      Given a source GID, return a path from the source gid
 *	to every other port on the subnet.  It is assumed that the
 *	subnet is fully connected.  Only one path per port on the subnet
 *	is returned.
 *
 * This interface blocks.
 *
 * Arguments:
 * ibmf_saa_handle	- handle returned from ibmf_sa_session_open()
 * sgid 		- source gid of path
 * pkey			- paritition of path.  This value may be wildcarded with
 *			  IBMF_SAA_PKEY_WC.
 * reversible		- if B_TRUE, ibmf will query only reversible paths;
 *			  see Infiniband Specification table 171
 * flags		- unused
 *
 * Output Arguments:
 * num_paths		- number of paths returned
 * length		- size of buffer returned
 * result		- pointer to buffer of path records returned in response
 *
 * Return values:
 * Error codes are the same as ibmf_sa_access() return values
 *
 * Upon successful completion, result points to a buffer containing the records.
 * and num_paths is the number of path records returned.  length is the size
 * in bytes of the buffer returned in result.  If there are no records or the
 * call failed the length is 0.
 *
 * The consumer is responsible for freeing the memory associated with result.
 */
int	ibmf_saa_paths_from_gid(
		ibmf_saa_handle_t	ibmf_saa_handle,
		ib_gid_t		sgid,
		ib_pkey_t		pkey,
		boolean_t		reversible,
		uint_t			flags,
		uint_t			*num_paths,
		size_t			*length,
		sa_path_record_t	**result);

/*
 * ibmf_saa_name_to_service_record:
 *	Given a service name, return the service records associated
 *	with it.
 *
 * This interface blocks.
 *
 * Input Arguments:
 * ibmf_saa_handle	- handle returned from ibmf_sa_session_open()
 * name			- service name, a null terminated string
 * p_key		- partition that the service is requested on.  This
 *			  value may be wildcarded with IBMF_SAA_PKEY_WC.
 * flags		- unused
 *
 * Output Arguments:
 * num_records		- number of service records returned
 * length		- size of buffer returned
 * result		- pointer to buffer of service records returned in
 *			  response
 *
 * Return values:
 * Error codes are the same as ibmf_sa_access() return values
 *
 * Upon successful completion, result points to a buffer containing the records.
 * and num_records is the number of service records returned.  length is the
 * size in bytes of the buffer returned in result.  If there are no records or
 * the call failed the length is 0.
 *
 * The consumer is responsible for freeing the memory associated with result.
 */
int	ibmf_saa_name_to_service_record(
		ibmf_saa_handle_t	ibmf_saa_handle,
		char			*service_name,
		ib_pkey_t		p_key,
		uint_t			flags,
		uint_t			*num_records,
		size_t			*length,
		sa_service_record_t	**result);

/*
 * ibmf_saa_id_to_service_record:
 *      Given a service id, return the service records associated
 *      with it.
 *
 * This interface blocks.
 *
 * Input Arguments:
 * ibmf_saa_handle	- handle returned from ibmf_sa_session_open()
 * id			- service id
 * p_key		- partition that the service is requested on.  This
 *			  value may be wildcarded with IBMF_SAA_PKEY_WC.
 * flags		- unused
 *
 * Output Arguments:
 * num_records		- number of service records returned
 * length		- size of buffer returned
 * result		- pointer to buffer of service records returned in
 *			  response
 *
 * Return values:
 * Error codes are the same as ibmf_sa_access() return values
 *
 * Upon successful completion, result points to a buffer containing the records.
 * and num_records is the number of service records returned.  length is the
 * size in bytes of the buffer returned in result.  If there are no records or
 * the call failed the length is 0.
 *
 * The consumer is responsible for freeing the memory associated with result.
 */
int	ibmf_saa_id_to_service_record(
		ibmf_saa_handle_t	ibmf_saa_handle,
		ib_svc_id_t		service_id,
		ib_pkey_t		p_key,
		uint_t			flags,
		uint_t			*num_records,
		size_t			*length,
		sa_service_record_t	**result);

/*
 * ibmf_saa_update_service_record
 *	Given a pointer to a service record, either insert or delete it
 *
 * This interface blocks.
 *
 * Input Arguments:
 * ibmf_saa_handle	- handle returned from ibmf_sa_session_open()
 * service_record	- service record is to be inserted or deleted.  To
 *			  delete a service record the GID, ID, P_Key, and
 *			  Service Key must match what is in the SA.
 * access_type		- indicates whether this is an insertion or deletion.
 *			  valid values are IBMF_SAA_UPDATE or IBMF_SAA_DELETE
 * flags		- unused
 *
 * Ouptut Arguments
 * none
 *
 * Return values:
 * Error codes are the same as ibmf_sa_access() return values
 */
int	ibmf_saa_update_service_record(
		ibmf_saa_handle_t	ibmf_saa_handle,
		sa_service_record_t	*service_record,
		ibmf_saa_access_type_t	access_type,
		uint_t			flags);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_IB_MGT_IBMF_IBMF_SAA_H */
