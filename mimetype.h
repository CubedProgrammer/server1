#ifndef Included_mimetype_h
#define Included_mimetype_h
int inittypes(const char*fname);
const char*mimetype_get(const char*key);
void freetypes(void);
#endif
