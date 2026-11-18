/* Bounded command grammar shared by the ARexx host and its tests. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rexx_bridge.h"
int ah_rexx_parse(const char *text,struct AmiHASSMessage *m)
{char words[6][96];unsigned int count=0,n;char quote;const unsigned char *p=(const unsigned char *)text;int i;char *end;long offset;if(!p||strlen(text)>512)return 0;memset(m,0,sizeof(*m));strcpy(m->owner,"AmiHASS.ARexx");
 while(*p){while(isspace(*p))++p;if(!*p)break;if(count==6)return 0;n=0;quote=(*p=='\''||*p=='"') ? *p++:0;while(*p&&(quote ? *p!=quote:!isspace(*p))){if(*p<32||*p==127||n>=95)return 0;words[count][n++]=(char)*p++;}words[count++][n]=0;if(quote){if(*p++!=quote)return 0;if(*p&&!isspace(*p))return 0;}}
 if(!count)return 0;for(i=0;words[0][i];++i)words[0][i]=(char)toupper((unsigned char)words[0][i]);
 if(!strcmp(words[0],"HELP")&&count==1)return 1;
 if(!strcmp(words[0],"STATUS")&&count==1)m->kind=AMIHASS_STATUS;
 else if(!strcmp(words[0],"QUERY")&&count==2&&amihass_entity_id(words[1])){m->kind=AMIHASS_QUERY;m->refresh=AMIHASS_QUERY_ONCE;strcpy(m->entity,words[1]);}
 else if(!strcmp(words[0],"ACTION")&&count==3&&amihass_entity_id(words[1])&&strlen(words[2])<20){m->kind=AMIHASS_ACTION;strcpy(m->entity,words[1]);strcpy(m->action,words[2]);}
 else if(!strcmp(words[0],"DEVICES")&&count<=3){m->kind=AMIHASS_DEVICES;if(count>1){if(strlen(words[1])>63)return 0;strcpy(m->filter,words[1]);}if(count==3){offset=strtol(words[2],&end,10);if(*end||offset<0||offset>100000)return 0;m->offset=offset;}}
 else if((!strcmp(words[0],"LIST")&&count<=4)||(!strcmp(words[0],"ENTITIES")&&count>=2&&count<=5)){m->kind=AMIHASS_LIST;i=1;if(!strcmp(words[0],"ENTITIES")){if(!amihass_device_id(words[1]))return 0;strcpy(m->entity,words[i++]);}if((unsigned int)i<count){if(strlen(words[i])>23)return 0;strcpy(m->domain,words[i++]);}if((unsigned int)i<count){if(strlen(words[i])>63)return 0;strcpy(m->filter,words[i++]);}if((unsigned int)i<count){offset=strtol(words[i],&end,10);if(*end||offset<0||offset>100000)return 0;m->offset=offset;}}
 else return 0;return 1;}
void ah_rexx_format(const struct AmiHASSMessage *m,char *out,unsigned int cap)
{unsigned int used;int i,n;if(!cap)return;out[0]=0;
 if(m->kind==AMIHASS_STATUS)snprintf(out,cap,"state=%s\nready=%ld\nsource=%s\nselected=%ld",m->item.state,(long)m->status.connected,m->status.server,(long)m->status.subscriptions);
 else if(m->kind==AMIHASS_QUERY||m->kind==AMIHASS_ACTION)snprintf(out,cap,"result=%ld\nid=%s\nname=%s\nstate=%s\nunit=%s",(long)m->result,m->item.id,m->item.name,m->item.state,m->item.unit);
 else{snprintf(out,cap,"total=%ld\noffset=%ld\ncount=%ld",(long)m->total,(long)m->offset,(long)m->count);for(i=0;i<m->count&&i<AMIHASS_PAGE;++i){used=strlen(out);if(used+2>=cap)break;n=snprintf(out+used,cap-used,"\n%s\t%s\t%s",m->items[i].id,m->items[i].name,m->items[i].state);if(n<0||(unsigned int)n>=cap-used)break;}}if(m->kind==AMIHASS_QUERY){for(i=0;i<m->count&&i<AMIHASS_PAGE;++i){used=strlen(out);if(used+2>=cap)break;n=snprintf(out+used,cap-used,"\n%s=%s",m->items[i].id,m->items[i].state);if(n<0||(unsigned int)n>=cap-used)break;}}out[cap-1]=0;}
