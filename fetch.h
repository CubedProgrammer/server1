#ifndef Included_fetch_h
#define Included_fetch_h
int servefile(cpcio_ostream os,const char*restrict dynamic,const char*restrict hostls,const char*restrict host,const char*restrict path);
int respond(cpcio_ostream os,const char*restrict dynamic,const char*first,const char*last);
void send_headers(char*buffer, cpcio_ostream os, cpcpcss_http_req res);
int fetch_dynamic(const char*restrict socketpath,const char*restrict path, size_t pathlen);
int validate(const char*path);
#endif
