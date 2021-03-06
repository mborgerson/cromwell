#include "boot.h"
//#include <string.h>

int ParseConfig(char *szBuffer, CONFIGENTRY *entry, EEPROMDATA *eeprom, char *szPath) {
	static char szLine[MAX_LINE];
	static char szTmp[MAX_LINE];
	char *ptr;

	memset(entry,0,sizeof(CONFIGENTRY));
	ptr = szBuffer;
	ptr = HelpGetToken(szBuffer,10);
	entry->nValid = 1;

	while(1) {
		memset(szLine,0x00,MAX_LINE);
		_strncpy(szLine,ptr,MAX_LINE);
		szLine[MAX_LINE-1]=0; 	// Make sure string is terminated
                
		if(strlen(ptr) < MAX_LINE) {
			if(_strncmp(ptr,"kernel ",strlen("kernel ")) == 0)  {
				HelpGetParm(szTmp, ptr);
				
				if (szPath!= NULL) 	sprintf(entry->szKernel,"%s",szPath);
				
				if (szTmp[0] != '/') sprintf(entry->szKernel,"%s%s",entry->szKernel,"/");
				
				sprintf(entry->szKernel,"%s%s",entry->szKernel,szTmp);
			}
			
			if(_strncmp(ptr,"initrd ",strlen("initrd ")) == 0) {
				HelpGetParm(szTmp, ptr);
				
				if (( szPath!= NULL) &&
					(_strncmp(szTmp, "no", strlen("no")) != 0))
					sprintf(entry->szInitrd,"%s",szPath);
				
				if (szTmp[0] != '/')
					sprintf(entry->szInitrd,"%s%s",entry->szInitrd,"/");
				
				sprintf(entry->szInitrd,"%s%s",entry->szInitrd,szTmp);
			}
			
			if(_strncmp(ptr,"append ",strlen("append ")) == 0)
				HelpGetParm(entry->szAppend, ptr);
		} else {
			entry->nValid = 0;
		}
		ptr = HelpGetToken(0,10);
		if(*ptr == 0) break;
		
	}
	chrreplace(entry->szInitrd, '\\', '/');
	chrreplace(entry->szKernel, '\\', '/');

	return entry->nValid;
}

