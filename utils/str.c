#include<string.h>
const char*double_null_list_search(const char*l,const char*s)
{
	for(; *l != '\0' && strcmp(l, s); l += strlen(l) + 1);
	return l;
}
