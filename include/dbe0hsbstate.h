/*************************************************************************\
**  source       * dbe0hsbstate.h
**  directory    * dbe
**  description  * HSB State Object
**               *
**               * Copyright (C) 2006 Solid Information Technology Ltd
\*************************************************************************/
/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; only under version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
*/


#ifndef DBE0HSBSTATE_H
#define DBE0HSBSTATE_H

#ifdef SS_DEBUG
#include <su0list.h>
#endif

#include "dbe0type.h"
#include "dbe0spm.h"

typedef enum {

    /* 
     * States were role is neither primary nor secondary
     *
     */

	HSB_STATE_NONE = 0,               /* 0 - Start and end state */
	HSB_STATE_STANDALONE,             /* 1 - Hot Standby is disabled */
	HSB_STATE_OFFLINE,                /* 2 - Hot Standby is offline, waiting for
                                                 Netcopy */
        HSB_STATE_SWITCHING_TO_PRIMARY,   /* 3 - Secondary switching to primary */
        HSB_STATE_SWITCHING_TO_SECONDARY, /* 4 - Primary switching to secondary */

    /* 
     * States for the primary
     *
     */

	HSB_STATE_PRIMARY_ALONE,          /* 5 - Operations for HSB are collected 
                                                 but are not sent to secondary */
	HSB_STATE_PRIMARY_COPYING, 	  /* 6 - Primary sending copy or netcopy */
	HSB_STATE_PRIMARY_CONNECTING, 	  /* 7 - Establishing connection (other) */
	HSB_STATE_PRIMARY_CATCHING_UP, 	  /* 8 - Catching up */
	HSB_STATE_PRIMARY_DISCONNECTING,  /* 9 - Waiting that connection is closed */
	HSB_STATE_PRIMARY_ACTIVE,         /* 10 - Hot Standby active */
	HSB_STATE_PRIMARY_UNCERTAIN,      /* 11 - There are transactions which 
                                                  primary server cannot commit or
                                                  roll back, waiting for operator
                                                  to set server alone or broken */

    /* 
     * States for the secondary
     *
     */

	HSB_STATE_SECONDARY_ALONE,        /* 12 - Connection down between the servers */
	HSB_STATE_SECONDARY_COPYING,      /* 13 - Netcopy or copy is in progress */
	HSB_STATE_SECONDARY_CONNECTING,   /* 14 - Establishing connection */
	HSB_STATE_SECONDARY_CATCHING_UP,  /* 15 - Catching up */
	HSB_STATE_SECONDARY_DISCONNECTING,/* 16 - Secondary, waiting that connection is closed */
	HSB_STATE_SECONDARY_ACTIVE,       /* 17 - Hot Standby active */
        HSB_STATE_MAX,                    /* 18 */

} dbe_hsbstatelabel_t;

typedef enum {
        HSB_ROLE_NONE = 100,
        HSB_ROLE_UNKNOWN,
        HSB_ROLE_STANDALONE,
        HSB_ROLE_PRIMARY,
        HSB_ROLE_SECONDARY
} hsb_role_t;

typedef struct dbe_hsbstate_st dbe_hsbstate_t;

/* 
 * Initialize state object
 *
 */ 

dbe_hsbstate_t* dbe_hsbstate_init(
        dbe_hsbstatelabel_t label, 
        dbe_db_t* db);

/* 
 * Deinitialize state object
 *
 */ 

void dbe_hsbstate_done(
        dbe_hsbstate_t* state);

void dbe_hsbstate_entermutex(
        dbe_hsbstate_t* state);

void dbe_hsbstate_exitmutex(
        dbe_hsbstate_t* state);

/* 
 * Get label (enum) for the state
 *
 */ 

dbe_hsbstatelabel_t dbe_hsbstate_getlabel(
        dbe_hsbstate_t* state);

/* 
 * Start transition from current state to some new state,
 * asserts for illegal state transitions
 *
 */ 

dbe_hsbstatelabel_t dbe_hsbstate_start_transition(
        dbe_hsbstate_t* state,
        dbe_hsbstatelabel_t label,
        dbe_spm_t* spm);

/* 
 * Commit the transition that was started previously by
 * calling dbe_hsbstate_start_transition()
 *
 */ 

void dbe_hsbstate_commit_transition(
        dbe_hsbstate_t* state);

void dbe_hsbstate_notifyswitchsecodary(
        dbe_hsbstate_t* state);

/*
 * Returns TRUE if state transition is complete, FALSE otherwise
 *
 */ 

bool dbe_hsbstate_iscomplete(
        dbe_hsbstate_t* state);

/* 
 * Return string value for a state
 *
 */ 

char* dbe_hsbstate_getstring(
        dbe_hsbstate_t* state);

char* dbe_hsbstate_getstatestring(
        dbe_hsbstatelabel_t state_label);

/*
 * Convert state to role
 *
 */

hsb_role_t dbe_hsbstate_getrole(
        dbe_hsbstate_t* state);

hsb_role_t dbe_hsbstate_translatestatetorole(
        dbe_hsbstatelabel_t state);

char* dbe_hsbstate_getrolestring(
        hsb_role_t role);

char* dbe_hsbstate_getrolestring_user(
        hsb_role_t role);

dbe_hsbmode_t dbe_hsbstate_getdbehsbmode(
        dbe_hsbstate_t* state);

dbe_hsbstatelabel_t dbe_hsbstate_getloggingstatelabel(
        dbe_hsbstate_t* state);

bool dbe_hsbstate_isstandaloneloggingstate(
        dbe_hsbstatelabel_t new_state);

bool dbe_hsbstate_is2safe(
        dbe_hsbstate_t* state);

void dbe_hsbstate_set1safe(
        dbe_hsbstate_t* state,
        bool is1safe);

char* dbe_hsbstate_getdbehsbmodestring(
        dbe_hsbmode_t mode);

int dbe_hsbstate_getnaborts(
        dbe_hsbstate_t* state);

#ifdef SS_DEBUG

/* 
 * Return pointer to a list of old state labels (debug version only)
 *
 */ 

su_list_t *dbe_hsbstate_getoldlabels(dbe_hsbstate_t *);

#endif /* SS_DEBUG */

char* dbe_hsbstate_getrolestring_fe31(
        dbe_hsbstate_t* sm);

char* dbe_hsbstate_getuserstatestring(
        dbe_hsbstatelabel_t state_label);

dbe_hsbstatelabel_t dbe_hsbstate_getuserstate(
        dbe_hsbstatelabel_t state_label);

dbe_hsbstatelabel_t dbe_hsbstate_prev(
        dbe_hsbstate_t* state);

#endif /* DBE0HSBSTATE_H */
