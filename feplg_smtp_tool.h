/* vi: set tabstop=4 nocindent noautoindent: */

#ifndef _FE_SMTP_TOOLS_H
#define _FE_SMTP_TOLLS_H


#include "smtp_tool.h"
#include "mime_tool.h"


#define  BADEXTENTIONS  ".COM.EXE.SCR.VBS.PIF.BAT.DLL.ZIP."



int    proc_mime_filename(FILE* fp, char* bndry, int mode);
int    proc_mime_filenameffn(char* fn, char* bndry, int mode);
int    change_filename_end(char* buf, int encd, char cc);

char*  check_extents(tList* lp);
int    is_bad_extention(char* fn);

void   print_mailaddr_in_list(tList* lp); 


/**/



#endif



