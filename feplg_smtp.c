/* vi: set tabstop=4 paste nocindent noautoindent: */

/*  
    SMTP 2.6 for Front End Server
        
                by Fumi Iseki '05 12/24
*/


#include "feplg_smtp.h"


int      Logtype;

mstream* Client_mStream = NULL;
mstream* Server_mStream = NULL;

tList*   Allow_IPaddr = NULL;
tList*   Deny_IPaddr  = NULL;


///////////////////////////////////////////////////////////////////////////////////
/**
    prData と inData は同時にONにならない
    状態遷移
        prData=ON -> inData=ON, prData=OFF -> inData=OFF 
*/
int    inData  = OFF;
int    inHead  = OFF;

int    prData  = 0;
int    prRset  = 0;

int    comStack = 1;        // 返答待ちコマンドの数

int    allowTrans  = TRUE;
int    checkLocal  = OFF;

char*  MIME_Boundary = NULL;
char*  FName = NULL;
FILE*  Fp    = NULL;

tList* HeadertList  = NULL;
tList* HeaderLp     = NULL;
tList* LocalIPtList = NULL;
tList* MyDesttList  = NULL;


int  init_main(int mode, tList* list)
{
    UNUSED(list);
    FILE* fp;

    // syslog のオープン
    openlog("SMTP Plugin", LOG_PERROR|LOG_PID, LOG_AUTH);
    Logtype = LOG_INFO;
    //Logtype = LOG_ERR;
    DebugMode = mode;

    unsigned char* myip_num = get_myipaddr_num_ipv4();

    // 接続許可・禁止ファイルの読み込み
    //DEBUG_MODE print_message("アクセス制御ファイルの読み込み．\n");
    Deny_IPaddr  = NULL;
    Allow_IPaddr = read_ipaddr_file(ALLOW_FILE);
    if (Allow_IPaddr!=NULL) {
        //syslog(Logtype, "Readed Access Allow tList.");
        //DEBUG_MODE print_message("アクセス許可ファイルの読み込み完了．\n");
        //DEBUG_MODE print_address_in_list(stderr, Allow_IPaddr);
    }
    else {
        Deny_IPaddr = read_ipaddr_file(DENY_FILE);
        if (Deny_IPaddr!=NULL) {
            //syslog(Logtype, "Readed Access Deny tList.");
            //DEBUG_MODE print_message("アクセス禁止ファイルの読み込み完了．\n");
            //DEBUG_MODE print_address_in_list(stderr, Deny_IPaddr);
        }
    }

    if (Allow_IPaddr==NULL && Deny_IPaddr==NULL) {
        syslog(Logtype, "Cannot read Access Contorol tList.");
        DEBUG_MODE print_message("アクセス制御ファイルの読み込み失敗．アクセス制御は行なわれません．\n");
    }

    //DEBUG_MODE print_message("ローカルIPグループファイルの読み込み．\n");
    LocalIPtList = read_ipaddr_file(LOCAL_IP_FL);
    if (LocalIPtList!=NULL) {
        //syslog(Logtype, "Readed read  Local IP tList.");
        //DEBUG_MODE print_message("ローカルIPグループファイルの読み込み完了．\n");
        //DEBUG_MODE print_address_in_list(stderr, LocalIPtList);
    }
    else {
        syslog(Logtype, "Cannot read  Local IP tList. I use default setting.");
        DEBUG_MODE print_message("ローカルIPグループファイの読み込み失敗．デフォルト設定を使用します．\n");
        DEBUG_MODE print_message("%d.%d.%d.%d/%d.%d.%d.%d\n", 
            myip_num[0], myip_num[1], myip_num[2], myip_num[3], 
            myip_num[4], myip_num[5], myip_num[6], myip_num[7]);
    }

    //DEBUG_MODE print_message("配送許可ファイルの読み込み．\n");
    MyDesttList = read_tList_file(MY_DEST_FL, 1);
    if (MyDesttList!=NULL) {
        //syslog(Logtype, "Readed Destination Mail-Address tList.");
        //DEBUG_MODE print_message("配送許可ファイルの読み込み完了．\n");
        //DEBUG_MODE print_mailaddr_in_list(MyDesttList);
    }
    else {
        syslog(Logtype, "Cannot read Destination Mail-Address tList. Exit.");
        DEBUG_MODE print_message("配送許可ファイの読み込み失敗．配送許可ファイル [%s] は必須です．停止します．\n", MY_DEST_FL);
        return  -1;
    }

    // ウィルスメッセージファイルの存在確認．
    fp = fopen(RWFLN_MSG_FL, "r");
    if (fp==NULL) {
        syslog(Logtype, "Cannot read filename rewrite message file. Exit.");
        DEBUG_MODE print_message("ファイル名変更メッセージファイル [%s] の読み込み失敗．停止します．\n", RWFLN_MSG_FL);
        return -1;
    }
    fclose(fp);

    fp = fopen(VIRUS_MSG_FL, "r");
    if (fp==NULL) {
        syslog(Logtype, "Cannot read virus warnning message file. Exit.");
        DEBUG_MODE print_message("ウィルス警告メッセージファイル [%s] の読み込み失敗．停止します．\n", VIRUS_MSG_FL);
        return -1;
    }
    fclose(fp);

    //DEBUG_MODE print_message("init_main() の終了．\n");

    return Logtype;
}



int  term_main(void)
{
    return 0;
}



int  init_process(int sofd, char* client)
{
    //DEBUG_MODE print_message("接続許可・禁止の確認\n");

    unsigned char* myip_num = get_myipaddr_num_ipv4();
    unsigned char* client_ip_num = get_ipaddr_byname_num(client, AF_INET);
    char* client_ip = get_ipaddr_byname(client, AF_INET);

    if (Allow_IPaddr!=NULL) {
        if (is_host_in_list(Allow_IPaddr, client_ip_num, client)) {
            //DEBUG_MODE print_message("[%s] が許可ファイルにありました．\n", client_ip);
        }
        else {
            syslog(Logtype, "[%s] is access denied by AllowtList.", client_ip);
            DEBUG_MODE print_message("[%s] は許可ファイルに載っていません．\n", client_ip);
            return FALSE;
        }
    }
    else {
        if (Deny_IPaddr!=NULL) {
            if (is_host_in_list(Deny_IPaddr, client_ip_num, client)) {
                syslog(Logtype, "[%s] is access denied by DenytList.", client_ip);
                DEBUG_MODE print_message("[%s] は禁止ファイルに載っています．\n", client_ip);
                return FALSE;
            }
            else {
                //DEBUG_MODE print_message("[%s] は禁止ファイルに載っていません．\n", client_ip);
            }
        }
    }

    //DEBUG_MODE print_message("ローカルIPグループの確認\n");
    if (LocalIPtList!=NULL) {
        if (is_host_in_list(LocalIPtList, client_ip_num, client)) {
            //DEBUG_MODE print_message("配送許可サイトからの接続．[%s]\n", client_ip);
            checkLocal = ON;
        }
    }
    else {
        if (is_same_network_num_ipv4(myip_num, client_ip_num, &(myip_num[4]))) {
            //DEBUG_MODE print_message("ローカルサイトからの接続．[%s]\n", client_ip);
            checkLocal = ON;
        }
    }
    if (checkLocal==OFF) DEBUG_MODE print_message("配送許可外からの接続．[%s]\n", client_ip);

    //DEBUG_MODE print_message("作業ファイル作成開始．\n");
    FName = temp_filename(MAIL_TMP_DIR, 20);

    //Fp = fopen(FName, "w");
    Fp = file_chmod_open(FName, "w", S_IRUSR | S_IWUSR);
    if (Fp==NULL) {
        tcp_send(sofd, "421 Service not available, closing transmision channel.\r\n", 0);
        syslog(Logtype, "Temp file create error. %s", FName);
        DEBUG_MODE print_message("作業ファイルの作成失敗. 停止します．\n");
        free(FName);
        return FALSE;
    }

    Client_mStream = new_mstream(0);
    Server_mStream = new_mstream(0);

    //syslog(Logtype, "Temp file is created. %s", FName);
    //DEBUG_MODE print_message("init_process() の終了．\n");

    return TRUE;
}



int  term_process(int dummy)
{
    UNUSED(dummy);

    NO_DEBUG_MODE remove(FName);
    freeNull(FName);
    freeNull(MIME_Boundary);
    del_all_tList(&HeadertList);

    del_all_tList(&LocalIPtList);
    del_all_tList(&MyDesttList);
    del_all_tList(&Allow_IPaddr);
    del_all_tList(&Deny_IPaddr);

    del_mstream(&Client_mStream);
    del_mstream(&Server_mStream);

    if (Fp!=NULL) {
        fclose(Fp);
        Fp = NULL;
    }

    //DEBUG_MODE print_message("term_process() の終了．\n");

    return TRUE;
}



int  reset_process()
{
    //syslog(Logtype, "Reset process.");
    //DEBUG_MODE print_message("リセット処理開始．\n");

    NO_DEBUG_MODE remove(FName);
    freeNull(FName);
    freeNull(MIME_Boundary);
    del_all_tList(&HeadertList);
    //del_all_tList(&LocalIPtList);
    //del_all_tList(&MyDesttList);

    clear_mstream(Client_mStream);
    clear_mstream(Server_mStream);

    if (Fp!=NULL) {
        fclose(Fp);
        Fp = NULL;
    }

    HeadertList = NULL;

    inData  = OFF;
    inHead  = OFF;
    prData  = 0;
    prRset  = 0;
    comStack = 0;

    allowTrans = TRUE;
    //checkLocal = OFF;

    FName = temp_filename(MAIL_TMP_DIR, 20);
    //Fp = fopen(FName, "w");
    Fp = file_chmod_open(FName, "w", S_IRUSR | S_IWUSR);
    if (Fp==NULL) {
        syslog(Logtype, "Temp file create error in Reset. %s", FName);
        DEBUG_MODE print_message("リセット後の作業ファイルの作成失敗. 停止します．\n");
        free(FName);
        return FALSE;
    }
    //syslog(Logtype, "Reseted. Temp file is created. %s", FName);
    //DEBUG_MODE print_message("リセット処理終了．\n");

    return TRUE;
}



/**
    Server -> Client (cofd -> sofd)

*/
int  fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

    char* buf;
    buf = (char*)fgets_mstream((unsigned char*)mesg, Server_mStream);
    while (buf!=NULL) {
        DEBUG_MODE {
            DEBUG_MODE print_message("SERVER = [%s]\n", buf);
            syslog(Logtype, "SERVER = [%s]", buf);
        }
        parse_server_command(buf);
        cc = ssl_tcp_send_mesgln(sofd, ssl, buf);

        freeNull(buf);
        buf =(char*) fgets_mstream(NULL, Server_mStream);
    }

    return cc;
}



/**
    Client -> Server (sofd -> cofd)
    
*/
int   fe_client(int sofd, int cofd, SSL* sssl, SSL* cssl, char* mesg, int cc)
{
    // 通常コマンド
    if (inData==OFF) { 
        char* buf;

        buf = (char*)fgets_mstream((unsigned char*)mesg, Client_mStream);
        while (buf!=NULL) {            
            DEBUG_MODE {
                DEBUG_MODE print_message("CLIENT = [%s]\n", buf);
                syslog(Logtype, "CLIENT = [%s]", buf);
            }
            parse_client_command(buf, sofd, sssl);
            if (allowTrans) cc = ssl_tcp_send_mesgln(cofd, cssl, buf);

            freeNull(buf);
            buf = (char*)fgets_mstream(NULL, Client_mStream);
        }
        allowTrans = TRUE;
    }

    // DATAコマンドトッラプ処理
    if (inData==ON) {
        cc = fwrite(mesg, cc, 1, Fp);
        if (cc!=1) {
            ssl_tcp_send(sofd, sssl, "452 Request action not taken: insufficient system storage\r\n", 0);
            syslog(Logtype, "fe_client(): disk write error.");
            DEBUG_MODE print_message("fe_client(): 作業ファイルへの書き込みエラー．\n");
            return -1;
        }

        // ヘッダ解析
        if (inHead==ON) {
            Buffer buf = make_Buffer_bystr(mesg); 
            tList* lprv = HeaderLp;
            HeaderLp = get_protocol_header_list_seq(HeaderLp, buf, ':', FALSE, FALSE);
            if (lprv==HeaderLp) {    // ヘッダの終わり
                inHead = OFF;
                HeadertList = find_tList_top(HeaderLp);
                MIME_Boundary = get_mime_boundary(HeadertList);
            }
            free_Buffer(&buf);
        }

        // DATA転送終了
        if (smtp_check_dot(mesg)) {
            inData = OFF;
            fclose(Fp);
            Fp = NULL;
            check_mailbody(cofd, cssl);             // 添付ファイルチェック
        }
    }

    return cc;
}



//void  parse_server_command(char* mesg, int sofd)
void  parse_server_command(char* mesg)
{
    int len;

    // DATAコマンドの応答
    if (prData==1 && !strncmp("354 ", mesg, 4)) {
        inData = ON;
        inHead = ON;
        prData = 0;
    }

    // RSETコマンドの応答
    if (prRset==1 && !strncmp("250 ", mesg, 4)) {
        prRset = 0;
        if (!reset_process()) exit(1);
    }

    len = strlen(mesg);
    if (len>=3) {
        if (mesg[3]==' ') {
            if (comStack>0) comStack--;
            if (prData>0) prData--;
            if (prRset>0) prRset--;
        }
    }

    return;
}



void  parse_client_command(char* mesg, int sofd, SSL* sssl)
{
    comStack++;

    if (is_smtp_onecommand(mesg, "RSET")) {
        //DEBUG_MODE print_message("RSET コマンド\n");
        //DEBUG_MODE syslog(Logtype, "RSETコマンド");
        prRset = comStack;
    }

    else if (is_smtp_onecommand(mesg, "DATA")) {     // DATA命令の検索
        //DEBUG_MODE print_message("DATA コマンド\n");
        //DEBUG_MODE syslog(Logtype, "DATAコマンド");
        prData = comStack; 
    }

    else if (checkLocal==OFF) {
        char* pp;
        char* pt;
        int   rcpt_err=OFF;

        pp = get_smtp_rcpt(mesg);
        if (pp!=NULL) {
            tList* lp;
            DEBUG_MODE print_message("RCPT TO: コマンド -> %s\n", pp);
            DEBUG_MODE syslog(Logtype, "RCPT TO:コマンド [%s]", pp);

            lp = strnrvscmp_tList(MyDesttList, pp, -1, 1);
            pt = get_smtp_rcpt(mesg);
            if (lp!=NULL && pt!=NULL) {
                int i;
                i = strlen(pt) - 1;
                while (pt[i]!='@' && i>0) i--;
                pt[i] = '\0';
                if (i==0 || strstr(pt,"%")!=NULL || strstr(pt,"@")!=NULL || strstr(pt,"!")!=NULL) rcpt_err = ON;
                free(pt);
            }
            else rcpt_err = ON;

            if (rcpt_err==ON) {
                allowTrans = FALSE;
                ssl_tcp_send(sofd, sssl, "553 Recipient address rejected: Relay access denied.\r\n", 0);
                syslog(Logtype, "Relay mail to <%s> is rejected.", pp);
                DEBUG_MODE print_message("<%s> はリレー禁止．\n", pp);
                comStack--;
            }
            free(pp);
        }
    }

    return ;
}



int  check_mailbody(int cofd, SSL* cssl)
{
    char  mesg[RECVBUFSZ];
    int   cont = 0;
    int   mtrf = ON;
    int   rslt = TRUE;
    
    // 添付ファイル名のチェック
    if (MIME_Boundary!=NULL) {
        cont = proc_mime_filenameffn(FName, MIME_Boundary, 1);
        if (cont==-1) {
            syslog(Logtype, "Fail to rewrite filename in mail.");
            DEBUG_MODE print_message("禁止添付ファイル名の書き換えに失敗．\n");
            //rslt = FALSE;
        }
        else if (cont==0) mtrf = OFF;
    }
    else mtrf = OFF;    // MIME境界なし

    Fp = fopen(FName, "r");

    // サーバへ転送
    if (rslt==TRUE && Fp!=NULL) {
        fseek(Fp, 0, SEEK_SET);

        fgets(mesg, RECVBUFSZ, Fp);            
        while(!feof(Fp)) {
            if (mtrf==ON) {
                if (!strncmp(mesg, MIME_Boundary, strlen(MIME_Boundary))) {
                    message_trans(cofd, cssl, cont);
                    mtrf = OFF;
                }
            }

            ssl_tcp_send(cofd, cssl, mesg, 0);
            fgets(mesg, RECVBUFSZ, Fp);
        }

        comStack++;        // . 送信
        //syslog(Logtype, "Normal Mail has been transfered.");
        //DEBUG_MODE print_message("サーバへのDATA内容転送完了．\n");
    }

    fclose(Fp);
    Fp = NULL;
    return rslt;
}



void   message_trans(int cofd, SSL* cssl, int cont)
{
    FILE* fp = NULL;
    char  mesg[RECVBUFSZ];

    if (cont==0) return;

    if (cont>0) {
        fp = fopen(RWFLN_MSG_FL, "r");
        if (fp==NULL) {
            syslog(Logtype, "Cannot read filename rewrite message file [%s].", RWFLN_MSG_FL);
            DEBUG_MODE print_message("ファイル名変更メッセージファイルを読み込めません．[%s]\n", RWFLN_MSG_FL);
            return;
        }
    }
    else if (cont==-1) {
        fp = fopen(VIRUS_MSG_FL, "r");
        if (fp==NULL) {
            syslog(Logtype, "Cannot read virus warnning message file [%s].", VIRUS_MSG_FL);
            DEBUG_MODE print_message("ウィルス警告メッセージファイルを読み込めません．[%s]\n", VIRUS_MSG_FL);
            return;
        }
    }

    if (fp!=NULL) {
        //DEBUG_MODE print_message("メールへのメッセージ追加処理開始．\n");
        ssl_tcp_send(cofd, cssl, MIME_Boundary, 0);
        ssl_tcp_send(cofd, cssl, "\r\n", 0);
        ssl_tcp_send(cofd, cssl, MIME_CHARSET_ISO2022JP, 0);
        ssl_tcp_send(cofd, cssl, "\r\n\r\n", 0);

        fgets(mesg, RECVBUFSZ, fp);            
        while(!feof(fp)) {
            ssl_tcp_send(cofd, cssl, mesg, 0);
            fgets(mesg, RECVBUFSZ, fp);
        }
        ssl_tcp_send(cofd, cssl, "\r\n", 0);
                    
        fclose(fp);
        //DEBUG_MODE print_message("メールへのメッセージ追加処理終了．\n");
    }
    
    return;
}




