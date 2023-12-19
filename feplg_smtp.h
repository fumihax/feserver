

#include "feplg.h"
#include "feplg_smtp_tool.h"




#define  ALLOW_FILE     "/usr/local/etc/feserver/smtp/allow.list"
#define  DENY_FILE      "/usr/local/etc/feserver/smtp/deny.list"

#define  LOCAL_IP_FL    "/usr/local/etc/feserver/smtp/localip.list"
#define  MY_DEST_FL     "/usr/local/etc/feserver/smtp/mydest.list"

#define  RWFLN_MSG_FL   "/usr/local/etc/feserver/smtp/rwmessage.text"
#define  VIRUS_MSG_FL   "/usr/local/etc/feserver/smtp/vrmessage.text"

#define  MAIL_TMP_DIR   "/var/feserver/smtp/"


void   parse_client_command(char* mesg, int sock, SSL* ssl);
void   parse_server_command(char* mesg);
int    check_mailbody(int, SSL*);
int    reset_process(void);
void   message_trans(int, SSL*, int);


