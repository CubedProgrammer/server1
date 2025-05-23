#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<cpcdt_date_struct.h>
#include"logger.h"
static FILE*global_logger_file_handle = NULL;
FILE*log_file_handle(void)
{
	return global_logger_file_handle;
}
int initialize_logger(const char*fname)
{
	global_logger_file_handle = fopen(fname, "ab");
	return global_logger_file_handle == NULL;
}
void log_header(void)
{
	cpcdt_date date = cpcdt_make_date(sec_since_epoch());
	fprintf(global_logger_file_handle, "Info at %d/%02d/%02d %02d:%02d:%02d\n", date->year, date->month, date->day, date->hr, date->min, (int)date->sec);
	free(date);
}
void log_data(const char*first,const char*last)
{
	fwrite(first, 1, last - first, global_logger_file_handle);
}
void log_cstr(const char*str)
{
	log_data(str, str + strlen(str));
}
void log_message_partial(const char*str)
{
	char lf = '\n';
	log_cstr(str);
	log_data(&lf, &lf+1);
}
void log_message_full(const char*str)
{
	log_header();
	log_message_partial(str);
	log_message_partial("");
	log_flush();
}
void log_sys_error(const char*str)
{
	log_header();
	log_sys_error_partial(str);
	log_message_partial("");
	log_flush();
}
void log_sys_error_partial(const char*str)
{
	log_cstr(str);
	log_cstr(": ");
	log_message_partial(strerror(errno));
}
void log_flush(void)
{
	fflush(global_logger_file_handle);
}
void finalize_logger(void)
{
	fclose(global_logger_file_handle);
}
