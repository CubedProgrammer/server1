#ifndef Included_logger_h
#define Included_logger_h
FILE*log_file_handle(void);
int initialize_logger(const char*fname);
void log_header(void);
void log_data(const char*first,const char*last);
void log_cstr(const char*str);
void log_message_partial(const char*str);
void log_message_full(const char*str);
void log_sys_error(const char*str);
void log_flush(void);
void finalize_logger(void);
#endif
