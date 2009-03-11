/*
 *	aprsc
 *
 *	(c) Heikki Hannikainen, OH7LZB <hessu@hes.iki.fi>
 *
 *     This program is licensed under the BSD license, which can be found
 *     in the file LICENSE.
 *	
 */

 /*
  *	Clientlist contains a list of connected clients, along with their
  *	verification status. It is updated when clients connect or disconnect,
  *	and lookups are made more often by the Q construct algorithm.
  *
  *	TODO: the clientlist should use a hash for quick lookups
  */

#include <string.h>

#include "clientlist.h"
#include "hmalloc.h"
#include "rwlock.h"

struct clientlist_t {
	struct clientlist_t *next;
	struct clientlist_t **prevp;
	
	char  username[16];     /* The callsign */
	int   validated;	/* Valid passcode given? */
	
	void *client_id;	/* DO NOT REFERENCE - just used for an ID */
};

struct clientlist_t *clientlist = NULL;
rwlock_t clientlist_lock = RWL_INITIALIZER;

/*
 *	Find a client by clientlist id - must hold either a read or write lock
 *	before calling!
 */

struct clientlist_t *clientlist_find_id(void *id)
{
	struct clientlist_t *cl;
	
	for (cl = clientlist; cl; cl = cl->next)
		if (cl->client_id == id)
			return cl;
	
	return NULL;
}

/*
 *	Add a client to the client list
 */

void clientlist_add(struct client_t *c)
{
	struct clientlist_t *cl;
	
	/* allocate and fill in */
	cl = hmalloc(sizeof(*cl));
	strncpy(cl->username, c->username, sizeof(cl->username));
	cl->username[sizeof(cl->username)-1] = 0;
	cl->validated = c->validated;
	
	/* get lock for list and insert */
	rwl_wrlock(&clientlist_lock);
	if (clientlist)
		clientlist->prevp = &cl->next;
	cl->next = clientlist;
	cl->prevp = &clientlist;
	cl->client_id = (void *)c;
	clientlist = cl;
	rwl_wrunlock(&clientlist_lock);
}

/*
 *	Remove a client from the client list
 */

void clientlist_remove(struct client_t *c)
{
	struct clientlist_t *cl;
	
	/* get lock for list, find, and remove */
	rwl_wrlock(&clientlist_lock);
	
	cl = clientlist_find_id((void *)c);
	if (cl) {
		if (cl->next)
			cl->next->prevp = cl->prevp;
		*cl->prevp = cl->next;
		
		hfree(cl);
	}
	
	rwl_wrunlock(&clientlist_lock);
}

/*
 *	Check if given usename is logged in and validated
 */

int clientlist_check_if_validated_client(char *username, int len)
{
	struct clientlist_t *cl;
	
	rwl_rdlock(&clientlist_lock);
	
	for (cl = clientlist; cl; cl = cl->next) {
		if (strncasecmp(username, cl->username, len) == 0
		  && strlen(cl->username) == len && cl->validated) {
			rwl_rdunlock(&clientlist_lock);
			return 1;
		}
	}
	
	rwl_rdunlock(&clientlist_lock);
	return 0;
}
