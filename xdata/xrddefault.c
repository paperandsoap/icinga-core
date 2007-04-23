/*****************************************************************************
 *
 * XRDDEFAULT.C - Default external state retention routines for Nagios
 *
 * Copyright (c) 1999-2006 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   04-07-2006
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


/*********** COMMON HEADER FILES ***********/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/nagios.h"
#include "../include/sretention.h"


/**** STATE INFORMATION SPECIFIC HEADER FILES ****/

#include "xrddefault.h"

extern host           *host_list;
extern service        *service_list;

extern char           *global_host_event_handler;
extern char           *global_service_event_handler;

extern char           *macro_x[MACRO_X_COUNT];

extern int            enable_notifications;
extern int            execute_service_checks;
extern int            accept_passive_service_checks;
extern int            execute_host_checks;
extern int            accept_passive_host_checks;
extern int            enable_event_handlers;
extern int            obsess_over_services;
extern int            obsess_over_hosts;
extern int            enable_flap_detection;
extern int            enable_failure_prediction;
extern int            process_performance_data;
extern int            check_service_freshness;
extern int            check_host_freshness;

extern int            use_retained_program_state;
extern int            use_retained_scheduling_info;
extern int            retention_scheduling_horizon;

extern unsigned long  modified_host_process_attributes;
extern unsigned long  modified_service_process_attributes;


char xrddefault_retention_file[MAX_FILENAME_LENGTH]="";
char xrddefault_temp_file[MAX_FILENAME_LENGTH]="";




/******************************************************************/
/********************* CONFIG INITIALIZATION  *********************/
/******************************************************************/

int xrddefault_grab_config_info(char *main_config_file){
	char *input=NULL;
	char *temp_ptr;
	mmapfile *thefile;
							      

	/* initialize the location of the retention file */
	strncpy(xrddefault_retention_file,DEFAULT_RETENTION_FILE,sizeof(xrddefault_retention_file)-1);
	strncpy(xrddefault_temp_file,DEFAULT_TEMP_FILE,sizeof(xrddefault_temp_file)-1);
	xrddefault_retention_file[sizeof(xrddefault_retention_file)-1]='\x0';
	xrddefault_temp_file[sizeof(xrddefault_temp_file)-1]='\x0';

	/* open the main config file for reading */
	if((thefile=mmap_fopen(main_config_file))==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open main configuration file '%s' for reading!\n",main_config_file);
#endif
		return ERROR;
	        }

	/* read in all lines from the main config file */
	while(1){

		/* free memory */
		free(input);

		/* read the next line */
		if((input=mmap_fgets(thefile))==NULL)
			break;

		strip(input);

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0')
			continue;

		temp_ptr=my_strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* temp file definition */
		if(!strcmp(temp_ptr,"temp_file")){
			temp_ptr=my_strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;
			strncpy(xrddefault_temp_file,temp_ptr,sizeof(xrddefault_temp_file)-1);
			xrddefault_temp_file[sizeof(xrddefault_temp_file)-1]='\x0';
			}

		/* retention file location */
		else if(!strcmp(temp_ptr,"xrddefault_retention_file") || !strcmp(temp_ptr,"state_retention_file")){
			temp_ptr=my_strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;
			strncpy(xrddefault_retention_file,temp_ptr,sizeof(xrddefault_retention_file)-1);
			xrddefault_retention_file[sizeof(xrddefault_retention_file)-1]='\x0';
			}
	        }

	/* free memory and close the file */
	free(input);
	mmap_fclose(thefile);

	/* save the retention file macro */
	if(macro_x[MACRO_RETENTIONDATAFILE]!=NULL)
		free(macro_x[MACRO_RETENTIONDATAFILE]);
	macro_x[MACRO_RETENTIONDATAFILE]=(char *)strdup(xrddefault_retention_file);
	if(macro_x[MACRO_RETENTIONDATAFILE]!=NULL)
		strip(macro_x[MACRO_RETENTIONDATAFILE]);

	return OK;
        }


/******************************************************************/
/**************** DEFAULT STATE OUTPUT FUNCTION *******************/
/******************************************************************/

int xrddefault_save_state_information(char *main_config_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_file[MAX_FILENAME_LENGTH];
	time_t current_time;
	int result=OK;
	FILE *fp=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;
	int x, fd=0;

#ifdef DEBUG0
	printf("xrddefault_save_state_information() start\n");
#endif

	/* grab config info */
	if(xrddefault_grab_config_info(main_config_file)==ERROR){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Failed to grab configuration information for retention data!\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xrddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((fd=mkstemp(temp_file))==-1)
		return ERROR;
	fp=fdopen(fd,"w");
	if(fp==NULL){

		close(fd);
		unlink(temp_file);

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not open temp state retention file '%s' for writing!\n",temp_file);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* write version info to status file */
	fprintf(fp,"########################################\n");
	fprintf(fp,"#      NAGIOS STATE RETENTION FILE\n");
	fprintf(fp,"#\n");
	fprintf(fp,"# THIS FILE IS AUTOMATICALLY GENERATED\n");
	fprintf(fp,"# BY NAGIOS.  DO NOT MODIFY THIS FILE!\n");
	fprintf(fp,"########################################\n\n");

	time(&current_time);

	/* write file info */
	fprintf(fp,"info {\n");
	fprintf(fp,"\tcreated=%lu\n",current_time);
	fprintf(fp,"\tversion=%s\n",PROGRAM_VERSION);
	fprintf(fp,"\t}\n\n");

	/* save program state information */
	fprintf(fp,"program {\n");
	fprintf(fp,"\tmodified_host_attributes=%lu\n",modified_host_process_attributes);
	fprintf(fp,"\tmodified_service_attributes=%lu\n",modified_service_process_attributes);
	fprintf(fp,"\tenable_notifications=%d\n",enable_notifications);
	fprintf(fp,"\tactive_service_checks_enabled=%d\n",execute_service_checks);
	fprintf(fp,"\tpassive_service_checks_enabled=%d\n",accept_passive_service_checks);
	fprintf(fp,"\tactive_host_checks_enabled=%d\n",execute_host_checks);
	fprintf(fp,"\tpassive_host_checks_enabled=%d\n",accept_passive_host_checks);
	fprintf(fp,"\tenable_event_handlers=%d\n",enable_event_handlers);
	fprintf(fp,"\tobsess_over_services=%d\n",obsess_over_services);
	fprintf(fp,"\tobsess_over_hosts=%d\n",obsess_over_hosts);
	fprintf(fp,"\tcheck_service_freshness=%d\n",check_service_freshness);
	fprintf(fp,"\tcheck_host_freshness=%d\n",check_host_freshness);
	fprintf(fp,"\tenable_flap_detection=%d\n",enable_flap_detection);
	fprintf(fp,"\tenable_failure_prediction=%d\n",enable_failure_prediction);
	fprintf(fp,"\tprocess_performance_data=%d\n",process_performance_data);
	fprintf(fp,"\tglobal_host_event_handler=%s\n",(global_host_event_handler==NULL)?"":global_host_event_handler);
	fprintf(fp,"\tglobal_service_event_handler=%s\n",(global_service_event_handler==NULL)?"":global_service_event_handler);
	fprintf(fp,"\t}\n\n");

	/* save host state information */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		fprintf(fp,"host {\n");
		fprintf(fp,"\thost_name=%s\n",temp_host->name);
		fprintf(fp,"\tmodified_attributes=%lu\n",temp_host->modified_attributes);
		fprintf(fp,"\tcheck_command=%s\n",(temp_host->host_check_command==NULL)?"":temp_host->host_check_command);
		fprintf(fp,"\tevent_handler=%s\n",(temp_host->event_handler==NULL)?"":temp_host->event_handler);
		fprintf(fp,"\thas_been_checked=%d\n",temp_host->has_been_checked);
		fprintf(fp,"\tcheck_execution_time=%.3f\n",temp_host->execution_time);
		fprintf(fp,"\tcheck_latency=%.3f\n",temp_host->latency);
		fprintf(fp,"\tcheck_type=%d\n",temp_host->check_type);
		fprintf(fp,"\tcurrent_state=%d\n",temp_host->current_state);
		fprintf(fp,"\tlast_state=%d\n",temp_host->last_state);
		fprintf(fp,"\tlast_hard_state=%d\n",temp_host->last_hard_state);
		fprintf(fp,"\tplugin_output=%s\n",(temp_host->plugin_output==NULL)?"":temp_host->plugin_output);
		fprintf(fp,"\tperformance_data=%s\n",(temp_host->perf_data==NULL)?"":temp_host->perf_data);
		fprintf(fp,"\tlast_check=%lu\n",temp_host->last_check);
		fprintf(fp,"\tnext_check=%lu\n",temp_host->next_check);
		fprintf(fp,"\tcurrent_attempt=%d\n",temp_host->current_attempt);
		fprintf(fp,"\tmax_attempts=%d\n",temp_host->max_attempts);
		fprintf(fp,"\tnormal_check_interval=%d\n",temp_host->check_interval);
		fprintf(fp,"\tstate_type=%d\n",temp_host->state_type);
		fprintf(fp,"\tlast_state_change=%lu\n",temp_host->last_state_change);
		fprintf(fp,"\tlast_hard_state_change=%lu\n",temp_host->last_hard_state_change);
		fprintf(fp,"\tlast_time_up=%lu\n",temp_host->last_time_up);
		fprintf(fp,"\tlast_time_down=%lu\n",temp_host->last_time_down);
		fprintf(fp,"\tlast_time_unreachable=%lu\n",temp_host->last_time_unreachable);
		fprintf(fp,"\tnotified_on_down=%d\n",temp_host->notified_on_down);
		fprintf(fp,"\tnotified_on_unreachable=%d\n",temp_host->notified_on_unreachable);
		fprintf(fp,"\tlast_notification=%lu\n",temp_host->last_host_notification);
		fprintf(fp,"\tcurrent_notification_number=%d\n",temp_host->current_notification_number);
		fprintf(fp,"\tnotifications_enabled=%d\n",temp_host->notifications_enabled);
		fprintf(fp,"\tproblem_has_been_acknowledged=%d\n",temp_host->problem_has_been_acknowledged);
		fprintf(fp,"\tacknowledgement_type=%d\n",temp_host->acknowledgement_type);
		fprintf(fp,"\tactive_checks_enabled=%d\n",temp_host->checks_enabled);
		fprintf(fp,"\tpassive_checks_enabled=%d\n",temp_host->accept_passive_host_checks);
		fprintf(fp,"\tevent_handler_enabled=%d\n",temp_host->event_handler_enabled);
		fprintf(fp,"\tflap_detection_enabled=%d\n",temp_host->flap_detection_enabled);
		fprintf(fp,"\tfailure_prediction_enabled=%d\n",temp_host->failure_prediction_enabled);
		fprintf(fp,"\tprocess_performance_data=%d\n",temp_host->process_performance_data);
		fprintf(fp,"\tobsess_over_host=%d\n",temp_host->obsess_over_host);
		fprintf(fp,"\tis_flapping=%d\n",temp_host->is_flapping);
		fprintf(fp,"\tpercent_state_change=%.2f\n",temp_host->percent_state_change);

		fprintf(fp,"\tstate_history=");
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			fprintf(fp,"%s%d",(x>0)?",":"",temp_host->state_history[(x+temp_host->state_history_index)%MAX_STATE_HISTORY_ENTRIES]);
		fprintf(fp,"\n");

		fprintf(fp,"\t}\n\n");
	        }

	/* save service state information */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		fprintf(fp,"service {\n");
		fprintf(fp,"\thost_name=%s\n",temp_service->host_name);
		fprintf(fp,"\tservice_description=%s\n",temp_service->description);
		fprintf(fp,"\tmodified_attributes=%lu\n",temp_service->modified_attributes);
		fprintf(fp,"\tcheck_command=%s\n",(temp_service->service_check_command==NULL)?"":temp_service->service_check_command);
		fprintf(fp,"\tevent_handler=%s\n",(temp_service->event_handler==NULL)?"":temp_service->event_handler);
		fprintf(fp,"\thas_been_checked=%d\n",temp_service->has_been_checked);
		fprintf(fp,"\tcheck_execution_time=%.3f\n",temp_service->execution_time);
		fprintf(fp,"\tcheck_latency=%.3f\n",temp_service->latency);
		fprintf(fp,"\tcheck_type=%d\n",temp_service->check_type);
		fprintf(fp,"\tcurrent_state=%d\n",temp_service->current_state);
		fprintf(fp,"\tlast_state=%d\n",temp_service->last_state);
		fprintf(fp,"\tlast_hard_state=%d\n",temp_service->last_hard_state);
		fprintf(fp,"\tcurrent_attempt=%d\n",temp_service->current_attempt);
		fprintf(fp,"\tmax_attempts=%d\n",temp_service->max_attempts);
		fprintf(fp,"\tnormal_check_interval=%d\n",temp_service->check_interval);
		fprintf(fp,"\tretry_check_interval=%d\n",temp_service->retry_interval);
		fprintf(fp,"\tstate_type=%d\n",temp_service->state_type);
		fprintf(fp,"\tlast_state_change=%lu\n",temp_service->last_state_change);
		fprintf(fp,"\tlast_hard_state_change=%lu\n",temp_service->last_hard_state_change);
		fprintf(fp,"\tlast_time_ok=%lu\n",temp_service->last_time_ok);
		fprintf(fp,"\tlast_time_warning=%lu\n",temp_service->last_time_warning);
		fprintf(fp,"\tlast_time_unknown=%lu\n",temp_service->last_time_unknown);
		fprintf(fp,"\tlast_time_critical=%lu\n",temp_service->last_time_critical);
		fprintf(fp,"\tplugin_output=%s\n",(temp_service->plugin_output==NULL)?"":temp_service->plugin_output);
		fprintf(fp,"\tperformance_data=%s\n",(temp_service->perf_data==NULL)?"":temp_service->perf_data);
		fprintf(fp,"\tlast_check=%lu\n",temp_service->last_check);
		fprintf(fp,"\tnext_check=%lu\n",temp_service->next_check);
		fprintf(fp,"\tnotified_on_unknown=%d\n",temp_service->notified_on_unknown);
		fprintf(fp,"\tnotified_on_warning=%d\n",temp_service->notified_on_warning);
		fprintf(fp,"\tnotified_on_critical=%d\n",temp_service->notified_on_critical);
		fprintf(fp,"\tcurrent_notification_number=%d\n",temp_service->current_notification_number);
		fprintf(fp,"\tlast_notification=%lu\n",temp_service->last_notification);
		fprintf(fp,"\tnotifications_enabled=%d\n",temp_service->notifications_enabled);
		fprintf(fp,"\tactive_checks_enabled=%d\n",temp_service->checks_enabled);
		fprintf(fp,"\tpassive_checks_enabled=%d\n",temp_service->accept_passive_service_checks);
		fprintf(fp,"\tevent_handler_enabled=%d\n",temp_service->event_handler_enabled);
		fprintf(fp,"\tproblem_has_been_acknowledged=%d\n",temp_service->problem_has_been_acknowledged);
		fprintf(fp,"\tacknowledgement_type=%d\n",temp_service->acknowledgement_type);
		fprintf(fp,"\tflap_detection_enabled=%d\n",temp_service->flap_detection_enabled);
		fprintf(fp,"\tfailure_prediction_enabled=%d\n",temp_service->failure_prediction_enabled);
		fprintf(fp,"\tprocess_performance_data=%d\n",temp_service->process_performance_data);
		fprintf(fp,"\tobsess_over_service=%d\n",temp_service->obsess_over_service);
		fprintf(fp,"\tis_flapping=%d\n",temp_service->is_flapping);
		fprintf(fp,"\tpercent_state_change=%.2f\n",temp_service->percent_state_change);

		fprintf(fp,"\tstate_history=");
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			fprintf(fp,"%s%d",(x>0)?",":"",temp_service->state_history[(x+temp_service->state_history_index)%MAX_STATE_HISTORY_ENTRIES]);
		fprintf(fp,"\n");

		fprintf(fp,"\t}\n\n");
	        }

	fclose(fp);

	/* move the temp file to the retention file (overwrite the old retention file) */
	if(my_rename(temp_file,xrddefault_retention_file))
		return ERROR;


#ifdef DEBUG0
	printf("xrddefault_save_state_information() end\n");
#endif

	return result;
        }




/******************************************************************/
/***************** DEFAULT STATE INPUT FUNCTION *******************/
/******************************************************************/

int xrddefault_read_state_information(char *main_config_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_buffer2[MAX_INPUT_BUFFER];
	char *input=NULL;
	char *temp_ptr;
	mmapfile *thefile;
	char *host_name=NULL;
	char *service_description=NULL;
	int data_type=XRDDEFAULT_NO_DATA;
	int x;
	host *temp_host=NULL;
	service *temp_service=NULL;
	command *temp_command=NULL;
	char *var;
	char *val;
	char *ch;
	time_t creation_time;
	time_t current_time;
	int scheduling_info_is_ok=FALSE;

#ifdef DEBUG0
	printf("xrddefault_read_state_information() start\n");
#endif

	/* grab config info */
	if(xrddefault_grab_config_info(main_config_file)==ERROR){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Failed to grab configuration information for retention data!\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open the retention file for reading */
	if((thefile=mmap_fopen(xrddefault_retention_file))==NULL)
		return ERROR;


	/* read all lines in the retention file */
	while(1){

		/* free memory */
		free(input);

		/* read the next line */
		if((input=mmap_fgets(thefile))==NULL)
			break;

		strip(input);

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0')
			continue;

		else if(!strcmp(input,"info {"))
			data_type=XRDDEFAULT_INFO_DATA;
		else if(!strcmp(input,"program {"))
			data_type=XRDDEFAULT_PROGRAM_DATA;
		else if(!strcmp(input,"host {"))
			data_type=XRDDEFAULT_HOST_DATA;
		else if(!strcmp(input,"service {"))
			data_type=XRDDEFAULT_SERVICE_DATA;

		else if(!strcmp(input,"}")){

			switch(data_type){

			case XRDDEFAULT_INFO_DATA:
				break;

			case XRDDEFAULT_PROGRAM_DATA:

				/* adjust modified attributes if necessary */
				if(use_retained_program_state==FALSE){
					modified_host_process_attributes=MODATTR_NONE;
					modified_service_process_attributes=MODATTR_NONE;
				        }
				break;

			case XRDDEFAULT_HOST_DATA:

				if(temp_host!=NULL){

					/* adjust modified attributes if necessary */
					if(temp_host->retain_nonstatus_information==FALSE)
						temp_host->modified_attributes=MODATTR_NONE;

					/* calculate next possible notification time */
					if(temp_host->current_state!=HOST_UP && temp_host->last_host_notification!=(time_t)0)
						temp_host->next_host_notification=get_next_host_notification_time(temp_host,temp_host->last_host_notification);

					/* update host status */
					update_host_status(temp_host,FALSE);

					/* check for flapping */
					check_for_host_flapping(temp_host,FALSE);

					/* handle new vars added */
					if(temp_host->last_hard_state_change==(time_t)0)
						temp_host->last_hard_state_change=temp_host->last_state_change;
				        }

				free(host_name);
				host_name=NULL;
				temp_host=NULL;
				break;

			case XRDDEFAULT_SERVICE_DATA:

				if(temp_service!=NULL){

					/* adjust modified attributes if necessary */
					if(temp_service->retain_nonstatus_information==FALSE)
						temp_service->modified_attributes=MODATTR_NONE;

					/* calculate next possible notification time */
					if(temp_service->current_state!=STATE_OK && temp_service->last_notification!=(time_t)0)
						temp_service->next_notification=get_next_service_notification_time(temp_service,temp_service->last_notification);

					/* fix old vars */
					if(temp_service->has_been_checked==FALSE && temp_service->state_type==SOFT_STATE)
						temp_service->state_type=HARD_STATE;

					/* update service status */
					update_service_status(temp_service,FALSE);

					/* check for flapping */
					check_for_service_flapping(temp_service,FALSE);
					
					/* handle new vars added */
					if(temp_service->last_hard_state_change==(time_t)0)
						temp_service->last_hard_state_change=temp_service->last_state_change;
				        }

				free(host_name);
				host_name=NULL;
				free(service_description);
				service_description=NULL;
				temp_service=NULL;
				break;

			default:
				break;
			        }

			data_type=XRDDEFAULT_NO_DATA;
		        }

		else if(data_type!=XRDDEFAULT_NO_DATA){

			var=strtok(input,"=");
			val=strtok(NULL,"\n");
			if(val==NULL)
				continue;

			switch(data_type){

			case XRDDEFAULT_INFO_DATA:
				if(!strcmp(var,"created")){
					creation_time=strtoul(val,NULL,10);
					time(&current_time);
					if(current_time-creation_time<retention_scheduling_horizon)
						scheduling_info_is_ok=TRUE;
					else
						scheduling_info_is_ok=FALSE;
				        }
				break;

			case XRDDEFAULT_PROGRAM_DATA:
				if(!strcmp(var,"modified_host_attributes"))
					modified_host_process_attributes=strtoul(val,NULL,10);
				else if(!strcmp(var,"modified_service_attributes"))
					modified_service_process_attributes=strtoul(val,NULL,10);
				if(use_retained_program_state==TRUE){
					if(!strcmp(var,"enable_notifications")){
						if(modified_host_process_attributes & MODATTR_NOTIFICATIONS_ENABLED)
							enable_notifications=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"active_service_checks_enabled")){
						if(modified_service_process_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
							execute_service_checks=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"passive_service_checks_enabled")){
						if(modified_service_process_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
							accept_passive_service_checks=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"active_host_checks_enabled")){
						if(modified_host_process_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
							execute_host_checks=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"passive_host_checks_enabled")){
						if(modified_host_process_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
							accept_passive_host_checks=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"enable_event_handlers")){
						if(modified_host_process_attributes & MODATTR_EVENT_HANDLER_ENABLED)
							enable_event_handlers=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"obsess_over_services")){
						if(modified_service_process_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
							obsess_over_services=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"obsess_over_hosts")){
						if(modified_host_process_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
							obsess_over_hosts=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"check_service_freshness")){
						if(modified_service_process_attributes & MODATTR_FRESHNESS_CHECKS_ENABLED)
							check_service_freshness=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"check_host_freshness")){
						if(modified_host_process_attributes & MODATTR_FRESHNESS_CHECKS_ENABLED)
							check_host_freshness=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"enable_flap_detection")){
						if(modified_host_process_attributes & MODATTR_FLAP_DETECTION_ENABLED)
							enable_flap_detection=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"enable_failure_prediction")){
						if(modified_host_process_attributes & MODATTR_FAILURE_PREDICTION_ENABLED)
							enable_failure_prediction=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"process_performance_data")){
						if(modified_host_process_attributes & MODATTR_PERFORMANCE_DATA_ENABLED)
							process_performance_data=(atoi(val)>0)?TRUE:FALSE;
					        }
					else if(!strcmp(var,"global_host_event_handler")){
						if(modified_host_process_attributes & MODATTR_EVENT_HANDLER_COMMAND){

							/* make sure the check command still exists... */
							strncpy(temp_buffer2,val,sizeof(temp_buffer2));
							temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
							temp_ptr=my_strtok(temp_buffer2,"!");
							temp_command=find_command(temp_ptr);
							temp_ptr=strdup(val);

							if(temp_command!=NULL && temp_ptr!=NULL){
								free(global_host_event_handler);
								global_host_event_handler=temp_ptr;
							        }
						        }
					        }
					else if(!strcmp(var,"global_service_event_handler")){
						if(modified_service_process_attributes & MODATTR_EVENT_HANDLER_COMMAND){

							/* make sure the check command still exists... */
							strncpy(temp_buffer2,val,sizeof(temp_buffer2));
							temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
							temp_ptr=my_strtok(temp_buffer2,"!");
							temp_command=find_command(temp_ptr);
							temp_ptr=strdup(val);

							if(temp_command!=NULL && temp_ptr!=NULL){
								free(global_service_event_handler);
								global_service_event_handler=temp_ptr;
							        }
						        }
					        }
				        }
				break;

			case XRDDEFAULT_HOST_DATA:
				if(!strcmp(var,"host_name")){
					host_name=strdup(val);
					temp_host=find_host(host_name);
				        }
				else if(temp_host!=NULL){
					if(!strcmp(var,"modified_attributes"))
						temp_host->modified_attributes=strtoul(val,NULL,10);
					if(temp_host->retain_status_information==TRUE){
						if(!strcmp(var,"has_been_checked"))
							temp_host->has_been_checked=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"check_execution_time"))
							temp_host->execution_time=strtod(val,NULL);
						else if(!strcmp(var,"check_latency"))
							temp_host->latency=strtod(val,NULL);
						else if(!strcmp(var,"check_type"))
							temp_host->check_type=atoi(val);
						else if(!strcmp(var,"current_state"))
							temp_host->current_state=atoi(val);
						else if(!strcmp(var,"last_state"))
							temp_host->last_state=atoi(val);
						else if(!strcmp(var,"last_hard_state"))
							temp_host->last_hard_state=atoi(val);
						else if(!strcmp(var,"plugin_output")){
							strncpy(temp_host->plugin_output,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_host->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"performance_data")){
							strncpy(temp_host->perf_data,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_host->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"last_check"))
							temp_host->last_check=strtoul(val,NULL,10);
						else if(!strcmp(var,"next_check")){
							if(use_retained_scheduling_info==TRUE && scheduling_info_is_ok==TRUE)
								temp_host->next_check=strtoul(val,NULL,10);
						        }
						else if(!strcmp(var,"current_attempt"))
							temp_host->current_attempt=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"state_type"))
							temp_host->state_type=atoi(val);
						else if(!strcmp(var,"last_state_change"))
							temp_host->last_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_hard_state_change"))
							temp_host->last_hard_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_up"))
							temp_host->last_time_up=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_down"))
							temp_host->last_time_down=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_unreachable"))
							temp_host->last_time_unreachable=strtoul(val,NULL,10);
						else if(!strcmp(var,"notified_on_down"))
							temp_host->notified_on_down=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"notified_on_unreachable"))
							temp_host->notified_on_unreachable=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"last_notification"))
							temp_host->last_host_notification=strtoul(val,NULL,10);
						else if(!strcmp(var,"current_notification_number"))
							temp_host->current_notification_number=atoi(val);
						else if(!strcmp(var,"state_history")){
							temp_ptr=val;
							for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++){
								if((ch=my_strsep(&temp_ptr,","))!=NULL)
									temp_host->state_history[x]=atoi(ch);
								else
									break;
							        }
							temp_host->state_history_index=0;
						        }
					        }
					if(temp_host->retain_nonstatus_information==TRUE){
						if(!strcmp(var,"problem_has_been_acknowledged"))
							temp_host->problem_has_been_acknowledged=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"acknowledgement_type"))
							temp_host->acknowledgement_type=atoi(val);
						else if(!strcmp(var,"notifications_enabled")){
							if(temp_host->modified_attributes & MODATTR_NOTIFICATIONS_ENABLED)
								temp_host->notifications_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"active_checks_enabled")){
							if(temp_host->modified_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
								temp_host->checks_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"passive_checks_enabled")){
							if(temp_host->modified_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
								temp_host->accept_passive_host_checks=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"event_handler_enabled")){
							if(temp_host->modified_attributes & MODATTR_EVENT_HANDLER_ENABLED)
								temp_host->event_handler_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"flap_detection_enabled")){
							if(temp_host->modified_attributes & MODATTR_FLAP_DETECTION_ENABLED)
								temp_host->flap_detection_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"failure_prediction_enabled")){
							if(temp_host->modified_attributes & MODATTR_FAILURE_PREDICTION_ENABLED)
								temp_host->failure_prediction_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"process_performance_data")){
							if(temp_host->modified_attributes & MODATTR_PERFORMANCE_DATA_ENABLED)
								temp_host->process_performance_data=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"obsess_over_host")){
							if(temp_host->modified_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
								temp_host->obsess_over_host=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"check_command")){
							if(temp_host->modified_attributes & MODATTR_CHECK_COMMAND){

								/* make sure the check command still exists... */
								strncpy(temp_buffer2,val,sizeof(temp_buffer2));
								temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
								temp_ptr=my_strtok(temp_buffer2,"!");
								temp_command=find_command(temp_ptr);
								temp_ptr=strdup(val);

								if(temp_command!=NULL && temp_ptr!=NULL){
									free(temp_host->host_check_command);
									temp_host->host_check_command=temp_ptr;
								        }
							        }
						        }
						else if(!strcmp(var,"event_handler")){
							if(temp_host->modified_attributes & MODATTR_EVENT_HANDLER_COMMAND){

								/* make sure the check command still exists... */
								strncpy(temp_buffer2,val,sizeof(temp_buffer2));
								temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
								temp_ptr=my_strtok(temp_buffer2,"!");
								temp_command=find_command(temp_ptr);
								temp_ptr=strdup(val);

								if(temp_command!=NULL && temp_ptr!=NULL){
									free(temp_host->event_handler);
									temp_host->event_handler=temp_ptr;
								        }
							        }
						        }
						else if(!strcmp(var,"normal_check_interval")){
							if(temp_host->modified_attributes & MODATTR_NORMAL_CHECK_INTERVAL && atoi(val)>=0)
								temp_host->check_interval=atoi(val);
						        }
						else if(!strcmp(var,"max_attempts")){
							if(temp_host->modified_attributes & MODATTR_MAX_CHECK_ATTEMPTS && atoi(val)>=1){
								
								temp_host->max_attempts=atoi(val);

								/* adjust current attempt number if in a hard state */
								if(temp_host->state_type==HARD_STATE && temp_host->current_state!=HOST_UP && temp_host->current_attempt>1)
									temp_host->current_attempt=temp_host->max_attempts;
							        }
						        }
					        }

				        }
				break;

			case XRDDEFAULT_SERVICE_DATA:
				if(!strcmp(var,"host_name")){
					host_name=strdup(val);
					temp_service=find_service(host_name,service_description);
				        }
				else if(!strcmp(var,"service_description")){
					service_description=strdup(val);
					temp_service=find_service(host_name,service_description);
				        }
				else if(temp_service!=NULL){
					if(!strcmp(var,"modified_attributes"))
						temp_service->modified_attributes=strtoul(val,NULL,10);
					if(temp_service->retain_status_information==TRUE){
						if(!strcmp(var,"has_been_checked"))
							temp_service->has_been_checked=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"check_execution_time"))
							temp_service->execution_time=strtod(val,NULL);
						else if(!strcmp(var,"check_latency"))
							temp_service->latency=strtod(val,NULL);
						else if(!strcmp(var,"check_type"))
							temp_service->check_type=atoi(val);
						else if(!strcmp(var,"current_state"))
							temp_service->current_state=atoi(val);
						else if(!strcmp(var,"last_state"))
							temp_service->last_state=atoi(val);
						else if(!strcmp(var,"last_hard_state"))
							temp_service->last_hard_state=atoi(val);
						else if(!strcmp(var,"current_attempt"))
							temp_service->current_attempt=atoi(val);
						else if(!strcmp(var,"state_type"))
							temp_service->state_type=atoi(val);
						else if(!strcmp(var,"last_state_change"))
							temp_service->last_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_hard_state_change"))
							temp_service->last_hard_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_ok"))
							temp_service->last_time_ok=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_warning"))
							temp_service->last_time_warning=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_unknown"))
							temp_service->last_time_unknown=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_time_critical"))
							temp_service->last_time_critical=strtoul(val,NULL,10);
						else if(!strcmp(var,"plugin_output")){
							strncpy(temp_service->plugin_output,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"performance_data")){
							strncpy(temp_service->perf_data,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_service->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"last_check"))
							temp_service->last_check=strtoul(val,NULL,10);
						else if(!strcmp(var,"next_check")){
							if(use_retained_scheduling_info==TRUE && scheduling_info_is_ok==TRUE)
								temp_service->next_check=strtoul(val,NULL,10);
						        }
						else if(!strcmp(var,"notified_on_unknown"))
							temp_service->notified_on_unknown=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"notified_on_warning"))
							temp_service->notified_on_warning=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"notified_on_critical"))
							temp_service->notified_on_critical=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"current_notification_number"))
							temp_service->current_notification_number=atoi(val);
						else if(!strcmp(var,"last_notification"))
							temp_service->last_notification=strtoul(val,NULL,10);

						else if(!strcmp(var,"state_history")){
							temp_ptr=val;
							for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++){
								if((ch=my_strsep(&temp_ptr,","))!=NULL)
									temp_service->state_history[x]=atoi(ch);
								else
									break;
							        }
							temp_service->state_history_index=0;
						        }
					        }
					if(temp_service->retain_nonstatus_information==TRUE){
						if(!strcmp(var,"problem_has_been_acknowledged"))
							temp_service->problem_has_been_acknowledged=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"acknowledgement_type"))
							temp_service->acknowledgement_type=atoi(val);
						else if(!strcmp(var,"notifications_enabled")){
							if(temp_service->modified_attributes & MODATTR_NOTIFICATIONS_ENABLED)
								temp_service->notifications_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"active_checks_enabled")){
							if(temp_service->modified_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
								temp_service->checks_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"passive_checks_enabled")){
							if(temp_service->modified_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
								temp_service->accept_passive_service_checks=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"event_handler_enabled")){
							if(temp_service->modified_attributes & MODATTR_EVENT_HANDLER_ENABLED)
								temp_service->event_handler_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"flap_detection_enabled")){
							if(temp_service->modified_attributes & MODATTR_FLAP_DETECTION_ENABLED)
								temp_service->flap_detection_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"failure_prediction_enabled")){
							if(temp_service->modified_attributes & MODATTR_FAILURE_PREDICTION_ENABLED)
								temp_service->failure_prediction_enabled=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"process_performance_data")){
							if(temp_service->modified_attributes & MODATTR_PERFORMANCE_DATA_ENABLED)
								temp_service->process_performance_data=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"obsess_over_service")){
							if(temp_service->modified_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
								temp_service->obsess_over_service=(atoi(val)>0)?TRUE:FALSE;
						        }
						else if(!strcmp(var,"check_command")){
							if(temp_service->modified_attributes & MODATTR_CHECK_COMMAND){

								/* make sure the check command still exists... */
								strncpy(temp_buffer2,val,sizeof(temp_buffer2));
								temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
								temp_ptr=my_strtok(temp_buffer2,"!");
								temp_command=find_command(temp_ptr);
								temp_ptr=strdup(val);

								if(temp_command!=NULL && temp_ptr!=NULL){
									free(temp_service->service_check_command);
									temp_service->service_check_command=temp_ptr;
								        }
							        }
						        }
						else if(!strcmp(var,"event_handler")){
							if(temp_service->modified_attributes & MODATTR_EVENT_HANDLER_COMMAND){

								/* make sure the check command still exists... */
								strncpy(temp_buffer2,val,sizeof(temp_buffer2));
								temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
								temp_ptr=my_strtok(temp_buffer2,"!");
								temp_command=find_command(temp_ptr);
								temp_ptr=strdup(val);

								if(temp_command!=NULL && temp_ptr!=NULL){
									free(temp_service->event_handler);
									temp_service->event_handler=temp_ptr;
								        }
							        }
						        }
						else if(!strcmp(var,"normal_check_interval")){
							if(temp_service->modified_attributes & MODATTR_NORMAL_CHECK_INTERVAL && atoi(val)>=0)
								temp_service->check_interval=atoi(val);
						        }
						else if(!strcmp(var,"retry_check_interval")){
							if(temp_service->modified_attributes & MODATTR_RETRY_CHECK_INTERVAL && atoi(val)>=0)
								temp_service->retry_interval=atoi(val);
						        }
						else if(!strcmp(var,"max_attempts")){
							if(temp_service->modified_attributes & MODATTR_MAX_CHECK_ATTEMPTS && atoi(val)>=1){
								
								temp_service->max_attempts=atoi(val);

								/* adjust current attempt number if in a hard state */
								if(temp_service->state_type==HARD_STATE && temp_service->current_state!=STATE_OK && temp_service->current_attempt>1)
									temp_service->current_attempt=temp_service->max_attempts;
							        }
						        }
					        }
				        }
				break;

			default:
				break;
			        }

		        }
	        }

	/* free memory and close the file */
	free(input);
	mmap_fclose(thefile);

#ifdef DEBUG0
	printf("xrddefault_read_state_information() end\n");
#endif

	return OK;
        }