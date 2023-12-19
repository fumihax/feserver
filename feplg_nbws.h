/* vi: set tabstop=4 nocindent noautoindent: */


#include "feplg.h"
#include "ipaddr_tool.h"
#include "tjson.h"



#define  ALLOW_FILE   "/usr/local/etc/feserver/ws/allow.list"
//#define  DENY_FILE    "/usr/local/etc/feserver/ws/deny.list"




struct  ws_info {
    char*  host;
    char*  inst_id;
    char*  lti_id;
    //
    char*  session;
    char*  message;
    char*  status;
    char*  username;
    char*  cell_id;
    char*  tags;
    char*  date;
};

