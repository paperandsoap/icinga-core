/*****************************************************************************
 *
 * BROKER.C - Event broker routines for Nagios
 *
 * Copyright (c) 2002-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-26-2003
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/nebcallbacks.h"
#include "../include/nebstructs.h"

extern int             event_broker_options;

extern time_t program_start;
extern int nagios_pid;
extern int daemon_mode;
extern time_t last_command_check;
extern time_t last_log_rotation;
extern int enable_notifications;
extern int execute_service_checks;
extern int accept_passive_service_checks;
extern int execute_host_checks;
extern int accept_passive_host_checks;
extern int enable_event_handlers;
extern int obsess_over_services;
extern int obsess_over_hosts;
extern int enable_flap_detection;
extern int enable_failure_prediction;
extern int process_performance_data;
extern int aggregate_status_updates;


#ifdef USE_EVENT_BROKER



/******************************************************************/
/************************* EVENT FUNCTIONS ************************/
/******************************************************************/


/* sends program data (starts, restarts, stops, etc.) to broker */
void broker_program_state(int type, int flags, int attr, struct timeval *timestamp){
	nebstruct_process_data ds;

	if(!(event_broker_options & BROKER_PROGRAM_STATE))
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_PROCESS_DATA,(void *)&ds);

	return;
        }


/* send timed event data to broker */
void broker_timed_event(int type, int flags, int attr, timed_event *event, struct timeval *timestamp){
	nebstruct_timed_event_data ds;
	service *temp_service;
	host *temp_host;

	if(!(event_broker_options & BROKER_TIMED_EVENTS))
		return;

	if(event==NULL)
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.event_type=event->event_type;
	ds.recurring=event->recurring;
	ds.run_time=event->run_time;
	ds.event_data=event->event_data;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_TIMED_EVENT_DATA,(void *)&ds);

	return;
        }



/* send log data to broker */
void broker_log_data(int type, int flags, int attr, char *data, unsigned long data_type, struct timeval *timestamp){
	nebstruct_log_data ds;

	if(!(event_broker_options & BROKER_LOGGED_DATA))
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.data_type=data_type;
	ds.data=data;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_LOG_DATA,(void *)&ds);

	return;
        }



/* send system command data to broker */
void broker_system_command(int type, int flags, int attr, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, struct timeval *timestamp){
	nebstruct_system_command_data ds;

	if(!(event_broker_options & BROKER_SYSTEM_COMMANDS))
		return;
	
	if(cmd==NULL)
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.timeout=timeout;
	ds.command_line=cmd;
	ds.early_timeout=early_timeout;
	ds.execution_time=exectime;
	ds.return_code=retcode;
	ds.output=output;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_SYSTEM_COMMAND_DATA,(void *)&ds);

	return;
        }



/* send event handler data to broker */
void broker_event_handler(int type, int flags, int attr, void *data, int state, int state_type, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, struct timeval *timestamp){
	service *temp_service=NULL;
	host *temp_host=NULL;
	nebstruct_event_handler_data ds;

	if(!(event_broker_options & BROKER_EVENT_HANDLERS))
		return;
	
	if(data==NULL)
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_EVENTHANDLER_SERVICE || type==NEBTYPE_EVENTHANDLER_GLOBAL_SERVICE){
		temp_service=(service *)data;
		ds.host_name=temp_service->host_name;
		ds.service_description=temp_service->description;
	        }
	else{
		temp_host=(host *)data;
		ds.host_name=temp_host->name;
		ds.service_description=NULL;
	        }
	ds.state=state;
	ds.state_type=state_type;
	ds.timeout=timeout;
	ds.command_line=cmd;
	ds.early_timeout=early_timeout;
	ds.execution_time=exectime;
	ds.return_code=retcode;
	ds.output=output;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_EVENT_HANDLER_DATA,(void *)&ds);

	return;
        }




/* send host check data to broker */
void broker_host_check(int type, int flags, int attr, host *hst, int check_type, int state, int state_type, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, char *perfdata, struct timeval *timestamp){
	nebstruct_host_check_data ds;

	if(!(event_broker_options & BROKER_HOST_CHECKS))
		return;
	
	if(hst==NULL)
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.host_name=hst->name;
	ds.check_type=check_type;
	ds.current_attempt=hst->current_attempt;
	ds.max_attempts=hst->max_attempts;
	ds.state=state;
	ds.state_type=state_type;
	ds.timeout=timeout;
	ds.command_line=cmd;
	ds.early_timeout=early_timeout;
	ds.execution_time=exectime;
	ds.latency=latency;
	ds.return_code=retcode;
	ds.output=output;
	ds.perf_data=perfdata;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_HOST_CHECK_DATA,(void *)&ds);

	return;
        }



/* send service check data to broker */
void broker_service_check(int type, int flags, int attr, service *svc, int check_type, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmd, struct timeval *timestamp){
	nebstruct_service_check_data ds;

	if(!(event_broker_options & BROKER_SERVICE_CHECKS))
		return;
	
	if(svc==NULL)
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.host_name=svc->host_name;
	ds.service_description=svc->description;
	ds.check_type=check_type;
	ds.current_attempt=svc->current_attempt;
	ds.max_attempts=svc->max_attempts;
	ds.state=svc->current_state;
	ds.state_type=svc->state_type;
	ds.timeout=timeout;
	ds.command_line=cmd;
	ds.early_timeout=early_timeout;
	ds.execution_time=exectime;
	ds.latency=latency;
	ds.return_code=retcode;
	ds.output=svc->plugin_output;
	ds.perf_data=svc->perf_data;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_SERVICE_CHECK_DATA,(void *)&ds);

	return;
        }



/* send comment data to broker */
void broker_comment_data(int type, int flags, int attr, int comment_type, int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long comment_id, struct timeval *timestamp){
	nebstruct_comment_data ds;

	if(!(event_broker_options & BROKER_COMMENT_DATA))
		return;
	
	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.comment_type=comment_type;
	ds.entry_type=entry_type;
	ds.host_name=host_name;
	ds.service_description=svc_description;
	ds.entry_time=entry_time;
	ds.author_name=author_name;
	ds.comment_data=comment_data;
	ds.persistent=persistent;
	ds.source=source;
	ds.expires=expires;
	ds.expire_time=expire_time;
	ds.comment_id=comment_id;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_COMMENT_DATA,(void *)&ds);

	return;
        }



/* send downtime data to broker */
void broker_downtime_data(int type, int flags, int attr, int downtime_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, struct timeval *timestamp){
	nebstruct_downtime_data ds;

	if(!(event_broker_options & BROKER_DOWNTIME_DATA))
		return;
	
	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.downtime_type=downtime_type;
	ds.host_name=host_name;
	ds.service_description=svc_description;
	ds.entry_time=entry_time;
	ds.author_name=author_name;
	ds.comment_data=comment_data;
	ds.start_time=start_time;
	ds.end_time=end_time;
	ds.fixed=fixed;
	ds.duration=duration;
	ds.triggered_by=triggered_by;
	ds.downtime_id=downtime_id;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_DOWNTIME_DATA,(void *)&ds);

	return;
        }



/* send flapping data to broker */
void broker_flapping_data(int type, int flags, int attr, int flapping_type, void *data, double percent_change, double threshold, struct timeval *timestamp){
	nebstruct_flapping_data ds;
	host *temp_host=NULL;
	service *temp_service=NULL;

	if(!(event_broker_options & BROKER_FLAPPING_DATA))
		return;

	if(data==NULL)
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.flapping_type=flapping_type;
	if(flapping_type==SERVICE_FLAPPING){
		temp_service=(service *)data;
		ds.host_name=temp_service->host_name;
		ds.service_description=temp_service->description;
		ds.comment_id=temp_service->flapping_comment_id;
	        }
	else{
		temp_host=(host *)data;
		ds.host_name=temp_host->name;
		ds.service_description=NULL;
		ds.comment_id=temp_host->flapping_comment_id;
	        }
	ds.percent_change=percent_change;
	ds.threshold=threshold;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_DOWNTIME_DATA,(void *)&ds);

	return;
        }



/* sends program status updates to broker */
void broker_program_status(int type, int flags, int attr, struct timeval *timestamp){
	nebstruct_program_status_data ds;

	if(!(event_broker_options & BROKER_STATUS_DATA))
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.program_start=program_start;
	ds.pid=nagios_pid;
	ds.daemon_mode=daemon_mode;
	ds.last_command_check=last_command_check;
	ds.last_log_rotation=last_log_rotation;
	ds.notifications_enabled=enable_notifications;
	ds.active_service_checks_enabled=execute_service_checks;
	ds.passive_service_checks_enabled=accept_passive_service_checks;
	ds.active_host_checks_enabled=execute_host_checks;
	ds.passive_host_checks_enabled=accept_passive_host_checks;
	ds.event_handlers_enabled=enable_event_handlers;
	ds.flap_detection_enabled=enable_flap_detection;
	ds.failure_prediction_enabled=enable_failure_prediction;
	ds.process_performance_data=process_performance_data;
	ds.obsess_over_hosts=obsess_over_hosts;
	ds.obsess_over_services=obsess_over_services;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_PROGRAM_STATUS_DATA,(void *)&ds);

	return;
        }



/* sends host status updates to broker */
void broker_host_status(int type, int flags, int attr, host *hst, struct timeval *timestamp){
	nebstruct_host_status_data ds;

	if(!(event_broker_options & BROKER_STATUS_DATA))
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.object_ptr=(void *)hst;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_HOST_STATUS_DATA,(void *)&ds);

	return;
        }



/* sends service status updates to broker */
void broker_service_status(int type, int flags, int attr, service *svc, struct timeval *timestamp){
	nebstruct_service_status_data ds;

	if(!(event_broker_options & BROKER_STATUS_DATA))
		return;

	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.object_ptr=(void *)svc;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_SERVICE_STATUS_DATA,(void *)&ds);

	return;
        }



/* send notification data to broker */
void broker_notification_data(int type, int flags, int attr, int notification_type, int reason_type, void *data, char *ack_author, char *ack_data,int contacts_notified, struct timeval *timestamp){
	nebstruct_notification_data ds;
	host *temp_host=NULL;
	service *temp_service=NULL;

	if(!(event_broker_options & BROKER_NOTIFICATIONS))
		return;
	
	/* fill struct with relevant data */
	ds.type=type;
	ds.flags=flags;
	ds.attr=attr;
	ds.timestamp=get_broker_timestamp(timestamp);

	ds.notification_type=notification_type;
	ds.reason_type=reason_type;
	if(notification_type==SERVICE_NOTIFICATION){
		temp_service=(service *)data;
		ds.host_name=temp_service->host_name;
		ds.service_description=temp_service->description;
		ds.state=temp_service->current_state;
		ds.output=temp_service->plugin_output;
	        }
	else{
		temp_host=(host *)data;
		ds.host_name=temp_host->name;
		ds.service_description=NULL;
		ds.state=temp_host->current_state;
		ds.output=temp_host->plugin_output;
	        }
	ds.ack_author=ack_author;
	ds.ack_data=ack_data;
	ds.contacts_notified=contacts_notified;

	/* make callbacks */
	neb_make_callbacks(NEBCALLBACK_NOTIFICATION_DATA,(void *)&ds);

	return;
        }


/******************************************************************/
/************************ UTILITY FUNCTIONS ***********************/
/******************************************************************/

/* gets timestamp for use by broker */
struct timeval get_broker_timestamp(struct timeval *timestamp){
	struct timeval tv;

	if(timestamp==NULL)
		gettimeofday(&tv,NULL);
	else
		tv=*timestamp;

	return tv;
        }



#endif

