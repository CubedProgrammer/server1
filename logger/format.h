#ifndef Included_format_h
#define Included_format_h
void log_fmtmsg_partial(const char*fmt,...);
void log_fmtmsg_partial_variadic(const char*fmt,va_list args);
void log_fmtmsg_full(const char*fmt,...);
void log_fmtmsg_full_variadic(const char*fmt,va_list args);
#endif
