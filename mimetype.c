#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"utils/str.h"
static char*global_mimetype_array = NULL;
int inittypes(const char*fname)
{
    int f = 0;
    FILE*fh = fopen(fname, "rb");
    if(fh != NULL)
	{
		size_t capa = 128;
		size_t cnt = 0;
		char*arr = malloc(capa);
		char*tmp = NULL;
		if(arr == NULL)
		{
			perror("malloc for mimetypes failed");
			f = 1;
		}
		else
		{
			size_t r = fread(arr, 1, capa, fh);
			while(r > 0)
			{
				if(cnt + r == capa)
				{
					tmp = malloc(capa + (capa >> 1));
					if(tmp != NULL)
					{
						memcpy(tmp, arr, capa);
						free(arr);
						capa += capa >> 1;
					}
					arr = tmp;
				}
				if(arr != NULL)
				{
					for(char*it = arr + cnt; it != arr + cnt + r; ++it)
					{
						if(isspace(*it))
						{
							*it = '\0';
						}
					}
					cnt += r;
					r = fread(arr + cnt, 1, capa - cnt, fh);
				}
				else
				{
					r = 0;
				}
			}
			if(arr != NULL)
			{
				if(capa > cnt + 1 + ((cnt + 1) >> 3) || cnt == capa)
				{
					global_mimetype_array = malloc(cnt + 1);
					if(global_mimetype_array != NULL)
					{
						memcpy(global_mimetype_array, arr, cnt);
						free(arr);
					}
					else
					{
						perror("malloc failed to allocate memory for mimetypes");
						f = 1;
					}
				}
				else
				{
					global_mimetype_array = arr;
				}
				if(global_mimetype_array != NULL)
				{
					global_mimetype_array[cnt] = '\0';
				}
			}
			else
			{
				perror("malloc failed to allocate memory for mimetypes");
				f = 1;
			}
		}
	}
    else
    {
    	perror("opening mimetype file failed");
    	f = 1;
	}
    return f;
}
const char*mimetype_get(const char*key)
{
	const char*type = double_null_list_search(global_mimetype_array, key);
	return*type ? type + strlen(type) + 1 : "text/plain";
}
void freetypes(void)
{
	free(global_mimetype_array);
	global_mimetype_array = NULL;
}
