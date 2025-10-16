/* vi: set tabstop=4 nocindent noautoindent: */

/*  
    Front End Server Plugin for ASN.1 (LDAP)
        
                by Fumi Iseki '21 07/16
*/


#include "feplg_asn1.h"


int    Logtype;
tList* Allow_IPaddr = NULL;

ringBuffer* ClientRingBuffer = NULL;
ringBuffer* ServerRingBuffer = NULL;

tDER* ServerDER = NULL;
tDER* ClientDER = NULL;


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

    ServerRingBuffer = NULL;
    ClientRingBuffer = NULL;
    ServerDER = NULL;
    ClientDER = NULL;

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

    if (ServerRingBuffer!=NULL) del_ringBuffer(&ServerRingBuffer);
    if (ClientRingBuffer!=NULL) del_ringBuffer(&ClientRingBuffer);
    if (ServerDER!=NULL) del_DER(&ServerDER);
    if (ClientDER!=NULL) del_DER(&ClientDER);

    ServerRingBuffer = NULL;
    ClientRingBuffer = NULL;
    ServerDER = NULL;
    ClientDER = NULL;

    return TRUE;
}



//
//    Server -> Client (cofd -> sofd)
//
int   fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

    cc = ssl_tcp_send(sofd, ssl, mesg, cc);

/**/
    print_message("S=>C: %d\n", cc);
    int cnt = 0;
    for (int i=0; i<cc; i++)  {
        print_message(" %02x", ((unsigned char*)mesg)[i]);
        cnt++;
        if (cnt%16==0) print_message("\n");
    }
    if (cnt%16!=0) print_message("\n");
    print_message("\n");
/**/

    //
    if (ServerRingBuffer==NULL) {
        ServerRingBuffer = new_ringBuffer(RECVBUFSZ);
    }
    put_ringBuffer(ServerRingBuffer, (unsigned char*)mesg, cc);

    int sz = get_DER_size(&(ServerRingBuffer->buf[ServerRingBuffer->spoint]), NULL);
    while (sz <= ServerRingBuffer->datasz) {
        Buffer* buf = get_Buffer_ringBuffer(ServerRingBuffer, sz);
        if (buf->buf!=NULL) {
            ServerDER = DER_parse(ServerDER, buf);
            del_Buffer(&buf);
        }
        if (ServerRingBuffer->datasz==0) break;
        sz = get_DER_size(&(ServerRingBuffer->buf[ServerRingBuffer->spoint]), NULL);
    }

    if (ServerDER!=NULL) {
        print_tDER(stdout, ServerDER);
        fprintf(stdout, "\n");
        fflush(stdout);
        if (ServerDER!=NULL) del_DER(&ServerDER);
    }
    return cc;
}



//
//    Client -> Server (sofd -> cofd)
//
int   fe_client(int dummy1, int cofd, SSL* dummy2, SSL* ssld, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

    cc = ssl_tcp_send(cofd, ssld, mesg, cc);

/**/
    print_message("C=>S: %d\n", cc);
    int cnt = 0;
    for (int i=0; i<cc; i++)  {
        print_message(" %02x", ((unsigned char*)mesg)[i]);
        cnt++;
        if (cnt%16==0) print_message("\n");
    }
    if (cnt%16!=0) print_message("\n");
    print_message("\n");
/**/

    //
    if (ClientRingBuffer==NULL) {
        ClientRingBuffer = new_ringBuffer(RECVBUFSZ);
    }
    put_ringBuffer(ClientRingBuffer, (unsigned char*)mesg, cc);

    int sz = get_DER_size(&(ClientRingBuffer->buf[ClientRingBuffer->spoint]), NULL);
    while (sz <= ClientRingBuffer->datasz) {
        Buffer* buf = get_Buffer_ringBuffer(ClientRingBuffer, sz);
        if (buf->buf!=NULL) {
            ClientDER = DER_parse(ClientDER, buf);
            del_Buffer(&buf);
        }
        if (ClientRingBuffer->datasz==0) break;
        sz = get_DER_size(&(ClientRingBuffer->buf[ClientRingBuffer->spoint]), NULL);
    }

    if (ClientDER!=NULL) {
        print_tDER(stdout, ClientDER);
        fprintf(stdout, "\n");
        fflush(stdout);
        if (ClientDER!=NULL) del_DER(&ClientDER);
    }

    return cc;
}


