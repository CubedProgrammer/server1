#include<stdarg.h>
#include<stdio.h>
#include"logger.h"
#include"format.h"
void log_fmtmsg_partial(const char*fmt,...)
{
	va_list args;
	va_start(args, fmt);
	log_fmtmsg_partial_variadic(fmt, args);
	va_end(args);
}
void log_fmtmsg_partial_variadic(const char*fmt,va_list args)
{
	vfprintf(log_file_handle(), fmt, args);
}
void log_fmtmsg_full(const char*fmt,...)
{
	va_list args;
	va_start(args, fmt);
	log_fmtmsg_full_variadic(fmt, args);
	va_end(args);
}
void log_fmtmsg_full_variadic(const char*fmt,va_list args)
{
	log_header();
	log_fmtmsg_partial_variadic(fmt, args);
	log_message_partial("");
}
