#include <stddef.h>
int nd_quote_path(char *dest,size_t limit,const char *path)
{
    size_t n=0;
    const unsigned char *p=(const unsigned char *)path;
    if (!dest || limit<3 || !path) return 0;
    dest[n++]='"';
    for (;*p;++p) {
        if (*p<32 || *p==127) return 0;
        if (*p=='*' || *p=='"') {
            if (n+2>=limit) return 0;
            dest[n++]='*';
        }
        if (n+1>=limit) return 0;
        dest[n++]=(char)*p;
    }
    if (n+2>limit) return 0;
    dest[n++]='"';dest[n]='\0';
    return 1;
}
