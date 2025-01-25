#ifndef Included_fetch_h
#define Included_fetch_h
int servefile(cpcio_ostream os,const char*restrict hostls,const char*restrict host,const char*restrict path);
int respond(cpcio_ostream os,const char*first,const char*last);
int validate(const char*path);
#endif
