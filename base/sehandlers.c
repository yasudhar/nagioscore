/*****************************************************************************
 *
 * SEHANDLERS.C - Service and host event and state handlers for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   09-11-2007
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/statusdata.h"
#include "../include/downtime.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/perfdata.h"
#include "../include/broker.h"


extern int             enable_event_handlers;
extern int             obsess_over_services;
extern int             obsess_over_hosts;

extern int             log_event_handlers;
extern int             log_host_retries;

extern unsigned long   next_event_id;

extern int             event_handler_timeout;
extern int             ocsp_timeout;
extern int             ochp_timeout;

extern char            *macro_x[MACRO_X_COUNT];

extern char            *global_host_event_handler;
extern char            *global_service_event_handler;
extern command         *global_host_event_handler_ptr;
extern command         *global_service_event_handler_ptr;

extern char            *ocsp_command;
extern char            *ochp_command;
extern command         *ocsp_command_ptr;
extern command         *ochp_command_ptr;

extern time_t          program_start;



/******************************************************************/
/************* OBSESSIVE COMPULSIVE HANDLER FUNCTIONS *************/
/******************************************************************/


/* handles service check results in an obsessive compulsive manner... */
int obsessive_compulsive_service_check_processor(service *svc){
	char *raw_command=NULL;
	char *processed_command=NULL;
	char *temp_buffer=NULL;
	host *temp_host=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

	log_debug_info(DEBUGL_FUNCTIONS,0,"obsessive_compulsive_service_check_processor()\n");

	if(svc==NULL)
		return ERROR;

	/* bail out if we shouldn't be obsessing */
	if(obsess_over_services==FALSE)
		return OK;
	if(svc->obsess_over_service==FALSE)
		return OK;

	/* if there is no valid command, exit */
	if(ocsp_command==NULL)
		return ERROR;

	/* find the associated host */
	if((temp_host=(host *)svc->host_ptr)==NULL)
		return ERROR;

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);
	grab_datetime_macros();
	grab_summary_macros(NULL);

	/* get the raw command line */
	get_raw_command_line(ocsp_command_ptr,ocsp_command,&raw_command,macro_options);
	if(raw_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS,2,"Raw obsessive compulsive service processor command line: %s\n",raw_command);

	/* process any macros in the raw command line */
	process_macros(raw_command,&processed_command,macro_options);
	if(processed_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS,2,"Processed obsessive compulsive service processor command line: %s\n",processed_command);

	/* run the command */
	my_system(processed_command,ocsp_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		asprintf(&temp_buffer,"Warning: OCSP command '%s' for service '%s' on host '%s' timed out after %d seconds\n",processed_command,svc->description,svc->host_name,ocsp_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* free memory */
	my_free((void **)&raw_command);
	my_free((void **)&processed_command);
	
	return OK;
        }



/* handles host check results in an obsessive compulsive manner... */
int obsessive_compulsive_host_check_processor(host *hst){
	char *raw_command=NULL;
	char *processed_command=NULL;
	char *temp_buffer=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

	log_debug_info(DEBUGL_FUNCTIONS,0,"obsessive_compulsive_host_check_processor()\n");

	if(hst==NULL)
		return ERROR;

	/* bail out if we shouldn't be obsessing */
	if(obsess_over_hosts==FALSE)
		return OK;
	if(hst->obsess_over_host==FALSE)
		return OK;

	/* if there is no valid command, exit */
	if(ochp_command==NULL)
		return ERROR;

	/* update macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_datetime_macros();
	grab_summary_macros(NULL);

	/* get the raw command line */
	get_raw_command_line(ochp_command_ptr,ochp_command,&raw_command,macro_options);
	if(raw_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS,2,"Raw obsessive compulsive host processor command line: %s\n",raw_command);

	/* process any macros in the raw command line */
	process_macros(raw_command,&processed_command,macro_options);
	if(processed_command=NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS,2,"Processed obsessive compulsive host processor command line: %s\n",processed_command);

	/* run the command */
	my_system(processed_command,ochp_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		asprintf(&temp_buffer,"Warning: OCHP command '%s' for host '%s' timed out after %d seconds\n",processed_command,hst->name,ochp_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* free memory */
	my_free((void **)&raw_command);
	my_free((void **)&processed_command);

	return OK;
        }




/******************************************************************/
/**************** SERVICE EVENT HANDLER FUNCTIONS *****************/
/******************************************************************/


/* handles changes in the state of a service */
int handle_service_event(service *svc){
	host *temp_host=NULL;

	log_debug_info(DEBUGL_FUNCTIONS,0,"handle_service_event()\n");

	if(svc==NULL)
		return ERROR;

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_statechange_data(NEBTYPE_STATECHANGE_END,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_STATECHANGE,(void *)svc,svc->current_state,svc->state_type,svc->current_attempt,svc->max_attempts,NULL);
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;
	if(svc->event_handler_enabled==FALSE)
		return OK;

	/* find the host */
	if((temp_host=(host *)svc->host_ptr)==NULL)
		return ERROR;

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);
	grab_datetime_macros();
	grab_summary_macros(NULL);

	/* run the global service event handler */
	run_global_service_event_handler(svc);

	/* run the event handler command if there is one */
	if(svc->event_handler!=NULL)
		run_service_event_handler(svc);

	/* check for external commands - the event handler may have given us some directives... */
	check_for_external_commands();

	return OK;
        }



/* runs the global service event handler */
int run_global_service_event_handler(service *svc){
	char *raw_command=NULL;
	char *processed_command=NULL;
	char *command_output=NULL;
	char *temp_buffer=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;


	log_debug_info(DEBUGL_FUNCTIONS,0,"run_global_service_event_handler()\n");

	if(svc==NULL)
		return ERROR;

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;

	/* a global service event handler command has not been defined */
	if(global_service_event_handler==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,1,"Running global event handler for service '%s' on host '%s'...\n",svc->description,svc->host_name);

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_service_event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(global_service_event_handler_ptr,global_service_event_handler,&raw_command,macro_options);
	if(raw_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Raw global service event handler command line: %s\n",raw_command);

	/* process any macros in the raw command line */
	process_macros(raw_command,&processed_command,macro_options);
	if(processed_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Processed global service event handler command line: %s\n",processed_command);

	if(log_event_handlers==TRUE){
		asprintf(&temp_buffer,"GLOBAL SERVICE EVENT HANDLER: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],macro_x[MACRO_SERVICESTATETYPE],macro_x[MACRO_SERVICEATTEMPT],global_service_event_handler);
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
		my_free((void **)&temp_buffer);
	        }

	/* run the command */
	result=my_system(processed_command,event_handler_timeout,&early_timeout,&exectime,&command_output,0);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		asprintf(&temp_buffer,"Warning: Global service event handler command '%s' timed out after %d seconds\n",processed_command,event_handler_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_service_event_handler,processed_command,command_output,NULL);
#endif

	/* free memory */
	my_free((void **)&command_output);
	my_free((void **)&raw_command);
	my_free((void **)&processed_command);

	return OK;
        }



/* runs a service event handler command */
int run_service_event_handler(service *svc){
	char *raw_command=NULL;
	char *processed_command=NULL;
	char *command_output=NULL;
	char *temp_buffer=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;


	log_debug_info(DEBUGL_FUNCTIONS,0,"run_service_event_handler()\n");

	if(svc==NULL)
		return ERROR;

	/* bail if there's no command */
	if(svc->event_handler==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,1,"Running event handler for service '%s' on host '%s'...\n",svc->description,svc->host_name);

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,svc->event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(svc->event_handler_ptr,svc->event_handler,&raw_command,macro_options);
	if(raw_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Raw service event handler command line: %s\n",raw_command);

	/* process any macros in the raw command line */
	process_macros(raw_command,&processed_command,macro_options);
	if(processed_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Processed service event handler command line: %s\n",processed_command);

	if(log_event_handlers==TRUE){
		asprintf(&temp_buffer,"SERVICE EVENT HANDLER: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],macro_x[MACRO_SERVICESTATETYPE],macro_x[MACRO_SERVICEATTEMPT],svc->event_handler);
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
		my_free((void **)&temp_buffer);
	        }

	/* run the command */
	result=my_system(processed_command,event_handler_timeout,&early_timeout,&exectime,&command_output,0);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		asprintf(&temp_buffer,"Warning: Service event handler command '%s' timed out after %d seconds\n",processed_command,event_handler_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,svc->event_handler,processed_command,command_output,NULL);
#endif

	/* free memory */
	my_free((void **)&command_output);
	my_free((void **)&raw_command);
	my_free((void **)&processed_command);

	return OK;
        }




/******************************************************************/
/****************** HOST EVENT HANDLER FUNCTIONS ******************/
/******************************************************************/


/* handles a change in the status of a host */
int handle_host_event(host *hst){

	log_debug_info(DEBUGL_FUNCTIONS,0,"handle_host_event()\n");

	if(hst==NULL)
		return ERROR;

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_statechange_data(NEBTYPE_STATECHANGE_END,NEBFLAG_NONE,NEBATTR_NONE,HOST_STATECHANGE,(void *)hst,hst->current_state,hst->state_type,hst->current_attempt,hst->max_attempts,NULL);
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;
	if(hst->event_handler_enabled==FALSE)
		return OK;

	/* update host macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_datetime_macros();
	grab_summary_macros(NULL);

	/* run the global host event handler */
	run_global_host_event_handler(hst);

	/* run the event handler command if there is one */
	if(hst->event_handler!=NULL)
		run_host_event_handler(hst);

	/* check for external commands - the event handler may have given us some directives... */
	check_for_external_commands();

	return OK;
        }


/* runs the global host event handler */
int run_global_host_event_handler(host *hst){
	char *raw_command=NULL;
	char *processed_command=NULL;
	char *command_output=NULL;
	char *temp_buffer=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;


	log_debug_info(DEBUGL_FUNCTIONS,0,"run_global_host_event_handler()\n");

	if(hst==NULL)
		return ERROR;

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;

	/* no global host event handler command is defined */
	if(global_host_event_handler==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,1,"Running global event handler for host '%s'..\n",hst->name);

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_host_event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(global_host_event_handler_ptr,global_host_event_handler,&raw_command,macro_options);
	if(raw_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Raw global host event handler command line: %s\n",raw_command);

	/* process any macros in the raw command line */
	process_macros(raw_command,&processed_command,macro_options);
	if(processed_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Processed global host event handler command line: %s\n",processed_command);

	if(log_event_handlers==TRUE){
		asprintf(&temp_buffer,"GLOBAL HOST EVENT HANDLER: %s;%s;%s;%s;%s\n",hst->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],global_host_event_handler);
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
		my_free((void **)&temp_buffer);
	        }

	/* run the command */
	result=my_system(processed_command,event_handler_timeout,&early_timeout,&exectime,&command_output,0);

	/* check for a timeout in the execution of the event handler command */
	if(early_timeout==TRUE){
		asprintf(&temp_buffer,"Warning: Global host event handler command '%s' timed out after %d seconds\n",processed_command,event_handler_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_host_event_handler,processed_command,command_output,NULL);
#endif

	/* free memory */
	my_free((void **)&command_output);
	my_free((void **)&raw_command);
	my_free((void **)&processed_command);

	return OK;
        }


/* runs a host event handler command */
int run_host_event_handler(host *hst){
	char *raw_command=NULL;
	char *processed_command=NULL;
	char *command_output=NULL;
	char *temp_buffer=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;


	log_debug_info(DEBUGL_FUNCTIONS,0,"run_host_event_handler()\n");

	if(hst==NULL)
		return ERROR;

	/* bail if there's no command */
	if(hst->event_handler==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,1,"Running event handler for host '%s'..\n",hst->name);

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,hst->event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(hst->event_handler_ptr,hst->event_handler,&raw_command,macro_options);
	if(raw_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Raw host event handler command line: %s\n",raw_command);

	/* process any macros in the raw command line */
	process_macros(raw_command,&processed_command,macro_options);
	if(processed_command==NULL)
		return ERROR;

	log_debug_info(DEBUGL_EVENTHANDLERS,2,"Processed host event handler command line: %s\n",processed_command);

	if(log_event_handlers==TRUE){
		asprintf(&temp_buffer,"HOST EVENT HANDLER: %s;%s;%s;%s;%s\n",hst->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],hst->event_handler);
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
		my_free((void **)&temp_buffer);
	        }

	/* run the command */
	result=my_system(processed_command,event_handler_timeout,&early_timeout,&exectime,&command_output,0);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		asprintf(&temp_buffer,"Warning: Host event handler command '%s' timed out after %d seconds\n",processed_command,event_handler_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,hst->event_handler,processed_command,command_output,NULL);
#endif

	/* free memory */
	my_free((void **)&command_output);
	my_free((void **)&raw_command);
	my_free((void **)&processed_command);

	return OK;
        }




/******************************************************************/
/****************** HOST STATE HANDLER FUNCTIONS ******************/
/******************************************************************/


/* top level host state handler - occurs after every host check (soft/hard and active/passive) */
int handle_host_state(host *hst){
	int state_change=FALSE;
	time_t current_time=0L;


	log_debug_info(DEBUGL_FUNCTIONS,0,"handle_host_state()\n");

	/* get current time */
	time(&current_time);

	/* obsess over this host check */
	obsessive_compulsive_host_check_processor(hst);

	/* update performance data */
	update_host_performance_data(hst);

	/* record latest time for current state */
	switch(hst->current_state){
	case HOST_UP:
		hst->last_time_up=current_time;
		break;
	case HOST_DOWN:
		hst->last_time_down=current_time;
		break;
	case HOST_UNREACHABLE:
		hst->last_time_unreachable=current_time;
		break;
	default:
		break;
	        }

	/* has the host state changed? */
	if(hst->last_state!=hst->current_state || hst->last_hard_state!=hst->current_state || (hst->current_state==HOST_UP && hst->state_type==SOFT_STATE))
		state_change=TRUE;

	/* if the host state has changed... */
	if(state_change==TRUE){

		/* update last state change times */
		hst->last_state_change=current_time;
		if(hst->state_type==HARD_STATE)
			hst->last_hard_state_change=current_time;

		/* update the event id */
		hst->last_event_id=hst->current_event_id;
		hst->current_event_id=next_event_id;
		next_event_id++;

		/* reset the acknowledgement flag if necessary */
		if(hst->acknowledgement_type==ACKNOWLEDGEMENT_NORMAL){

			hst->problem_has_been_acknowledged=FALSE;
			hst->acknowledgement_type=ACKNOWLEDGEMENT_NONE;

			/* remove any non-persistant comments associated with the ack */
			delete_host_acknowledgement_comments(hst);
		        }
		else if(hst->acknowledgement_type==ACKNOWLEDGEMENT_STICKY && hst->current_state==HOST_UP){

			hst->problem_has_been_acknowledged=FALSE;
			hst->acknowledgement_type=ACKNOWLEDGEMENT_NONE;

			/* remove any non-persistant comments associated with the ack */
			delete_host_acknowledgement_comments(hst);
		        }

		/* reset the next and last notification times */
		hst->last_host_notification=(time_t)0;
		hst->next_host_notification=(time_t)0;

		/* reset notification suppression option */
		hst->no_more_notifications=FALSE;

		/* write the host state change to the main log file */
		if(hst->state_type==HARD_STATE || (hst->state_type==SOFT_STATE && log_host_retries==TRUE))
			log_host_event(hst);

		/* check for start of flexible (non-fixed) scheduled downtime */
		if(hst->state_type==HARD_STATE)
			check_pending_flex_host_downtime(hst);

		/* notify contacts about the recovery or problem if its a "hard" state */
		if(hst->state_type==HARD_STATE)
			host_notification(hst,NOTIFICATION_NORMAL,NULL,NULL,NOTIFICATION_OPTION_NONE);

		/* handle the host state change */
		handle_host_event(hst);

		/* the host just recovered, so reset the current host attempt */
		if(hst->current_state==HOST_UP)
			hst->current_attempt=1;

		/* the host recovered, so reset the current notification number and state flags (after the recovery notification has gone out) */
		if(hst->current_state==HOST_UP){
			hst->current_notification_number=0;
			hst->notified_on_down=FALSE;
			hst->notified_on_unreachable=FALSE;
		        }
	        }

	/* else the host state has not changed */
	else{

		/* notify contacts if host is still down or unreachable */
		if(hst->current_state!=HOST_UP && hst->state_type==HARD_STATE)
			host_notification(hst,NOTIFICATION_NORMAL,NULL,NULL,NOTIFICATION_OPTION_NONE);

		/* if we're in a soft state and we should log host retries, do so now... */
		if(hst->state_type==SOFT_STATE && log_host_retries==TRUE)
			log_host_event(hst);
	        }

	return OK;
        }


