/* vi: set tabstop=4 nocindent noautoindent: */

/*  
    Front End Server Plugin for  No Operation 1.2
        
                by Fumi Iseki '05 09/21
*/


#include "feplg_nop.h"


int    Logtype;
tList* Allow_IPaddr = NULL;

 
int  init_main(int mode, tList* list)
{
    UNUSED(list);

    //openlog("FEServer NOP Plugin", LOG_PERROR|LOG_PID, LOG_AUTH);
    //Logtype = LOG_INFO;
    Logtype = NO_SYSLOG;
    DebugMode = mode;

    // 接続許可・禁止ファイルの読み込み
    //DEBUG_MODE print_message("アクセス制御ファイルの読み込み．\n");
    Allow_IPaddr = read_ipaddr_file(ALLOW_FILE);
    if (Allow_IPaddr!=NULL) {
        //syslog(Logtype, "Readed Access Allow tList.");
        //DEBUG_MODE print_message("アクセス許可ファイルの読み込み完了．\n");
        //DEBUG_MODE print_address_in_list(stderr, Allow_IPaddr);
    }
    else {
        syslog(Logtype, "Cannot read Access Contorol tList.");
        DEBUG_MODE print_message("アクセス制御ファイルの読み込み失敗．アクセス制御は行なわれません．\n");
    }

    return  Logtype;
}



int  term_main(void)
{
    return 0;
}



int  init_process(int dummy, char* client)
{
    UNUSED(dummy);

    //DEBUG_MODE print_message("接続許可・禁止の確認．\n");
    if (Allow_IPaddr!=NULL) {
        unsigned char* ip_num = get_ipaddr_byname_num(client, AF_INET);
        char* client_ip = get_ipaddr_byname(client, AF_INET);

        if (is_host_in_list(Allow_IPaddr, ip_num, client)) {
            //DEBUG_MODE print_message("[%s] が許可ファイルにありました．\n", client_ip);
        }
        else {
            syslog(Logtype, "[%s] is access denied by AllowtList.", client_ip);
            DEBUG_MODE print_message("[%s] は許可ファイルに載っていません．\n", client_ip);
            return FALSE;
        }
    }

    return TRUE;
}



int  term_process(int dummy)
{
    UNUSED(dummy);

    del_all_tList(&Allow_IPaddr);

    return TRUE;
}



//
//    Server -> Client (cofd -> sofd)   // fesrv is C
//
int   fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

    cc = ssl_tcp_send(sofd, ssl, mesg, cc);

    //print_message("\nS=>C:\n");
    //print_message("%s\n", mesg);

 /*
    int i, cnt = 0;
    for (i=0; i<cc; i++)  {
        print_message(" %02x", ((unsigned char*)mesg)[i]);
        cnt++;
        if (cnt%16==0) print_message("\n");
    }
    if (cnt%16!=0) print_message("\n");
 */

    return cc;
}



//
//    Client -> Server (sofd -> cofd)   // fesrv is S
//
int   fe_client(int dummy1, int cofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

/*
    Buffer buf = set_Buffer(mesg, cc);
    tList* lst = get_protocol_header_list(buf, ':', TRUE, TRUE);
    free_Buffer(&buf);

    tList* hst = search_key_tList(lst, "Host", 1);
    copy_s2Buffer("202.26.150.55:8002", &(hst->ldat.val));
    buf = restore_protocol_header(lst, ": ", TRUE, NULL);
    del_tList(&lst);

    cc = ssl_tcp_send_Buffer(cofd, ssl, &buf);
    
    print_message("%s\n", buf.buf);

    free_Buffer(&buf);
*/

    cc = ssl_tcp_send(cofd, ssl, mesg, cc);

    //print_message("\nC=>S: [%d]\n", cc);
    //print_message("%s\n", mesg);

/*
    int i, cnt = 0;
    for (i=0; i<cc; i++)  {
        print_message(" %02x", ((unsigned char*)mesg)[i]);
        cnt++;
        if (cnt%16==0) print_message("\n");
    }
    if (cnt%16!=0) print_message("\n");
*/
    //}

    return cc;
}


