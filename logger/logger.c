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
	char datebuf[91];
	cpcdt_date date = cpcdt_make_date(sec_since_epoch());
	cpcdt_readable_date(datebuf, date);
	free(date);
	fprintf(global_logger_file_handle, "Information at %s\n", datebuf);
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
	log_cstr(str);
	log_cstr(": ");
	log_message_partial(strerror(errno));
	log_message_partial("");
	log_flush();
}
void log_flush(void)
{
	fflush(global_logger_file_handle);
}
void finalize_logger(void)
{
	fclose(global_logger_file_handle);
}
