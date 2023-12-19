/* vi: set tabstop=4 nocindent noautoindent: */

/*  
    Front End Server Plugin for  Moodle & Jupyter Notebook Web Socket
        
                by Fumi.Iseki '22 01/16  v1.1.0  BSD License.
                              '22 07/07  v1.2.1  BSD License.
*/


#include "feplg_nbws.h"
#include "tjson.h"
#include "https_tool.h"

int      Logtype;
tList*   Allow_IPaddr = NULL;

tList*   HTTP_Header  = NULL;
tList*   HTTP_Host    = NULL;
tList*   HTTP_Length  = NULL;
tList*   HTTP_Data    = NULL;

tList*   flist        = NULL;


#define  XMLRPC_PATH_KEY     "XmlRpc_Path"
#define  XMLRPC_SERVICE_KEY  "XmlRpc_Service"
#define  XMLRPC_HTTPVER_KEY  "XmlRpc_HTTPver"
#define  XMLRPC_RESPONSE_KEY "XmlRpc_Response"

#define  LMS_SESSIONINFO_KEY "lms_sessioninfo="          // Instance id, LTI id
#define  LMS_SERVERURL_KEY   "lms_serverurl="            // Scheme:Name:PortNum
#define  LMS_SERVERPATH_KEY  "lms_serverpath="           // 
#define  LMS_RPCTOKEN_KEY    "lms_rpctoken="             // 

#define  SESSION_ID_KEY      "session_id="
#define  CONTENT_LENGTH      "\r\ncontent-length: " 


char*    SessionInfo    = NULL;
char*    ServerURL      = NULL;
char*    ServerToken    = NULL;
char*    ServerName     = NULL;
char*    ServerPath     = NULL;
int      ServerPort     = 80;
int      ServerTLS      = FALSE;

//
char*    XmlRpc_Path    = "/webservice/xmlrpc/server.php";
char*    XmlRpc_Service = "mod_lticontainer_write_nbdata";
char*    XmlRpc_HTTPver = "1.1";
int      XmlRpc_Response   = FALSE;



///////////////////////////////////////////////////////////////////////
// Web Socket Data

void  print_buf(FILE* fp, unsigned char* buf, int size)
{
    int i;
    for(i=0; i<size; i++) {
        fprintf(fp, "%02x ", buf[i]);
        if (i%8==7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
    fflush(fp);
}


tJson*  ws_json_parse(ringBuffer* ring)
{
    if (ring->datasz<=0) {
        ring->state = JBXL_NODATA;
        return NULL;
    }

    unsigned char knd = *ref_ringBuffer(ring, 0);
    if (knd!=0x81 && knd!=0x82) {
        ring->state = JBXL_INCOMPLETE;
        return NULL;
    }

    // ヘッダデータ
    tJson* json = NULL;
    unsigned char* pp = NULL;
    unsigned char* pm = NULL;
    int pos = 0, mask, i;
    long int len  = 0;

    // データの長さ
    mask = (int)((*ref_ringBuffer(ring, 1)) >> 7);
    len  = (long int)((*ref_ringBuffer(ring, 1)) & 0x7f);
    pos  = 2;
    if (len==126) {
        len = (long int)(*ref_ringBuffer(ring, 2))*256 + (long int)(*ref_ringBuffer(ring, 3));
        pos = 4;
    }
    else if (len==127) {
        len = (long int)(*ref_ringBuffer(ring, 2));
        for(i=3; i<10; i++) {
            len = len*256 + (long int)(*ref_ringBuffer(ring, i));
            pos = 10;
        }
    }
    // data size = pos + mask*4 + len

    // ボディデータ処理
    if (ring->datasz >= pos + mask*4 + len) {
        //
        seek_ringBuffer(ring, pos);
        if (mask==1) pm = get_ringBuffer(ring, 4);
        pp = get_ringBuffer(ring, len);   // get (len + 1) Byte
        pp[len] = '\0';
        //
        if (mask==1) {
            for (i=0; i<len; i++) {
                pp[i] = pp[i] ^ pm[i%4];
            }
        }

        unsigned char* px = pp;
        if (knd==0x82) {
            px += px[8];                // 推定処理．要確認 
            if (strncasecmp((const char*)px, "shell", 5)) {
                free(pp);
                if (pm!=NULL) free(pm);
                ring->state = JBXL_DATA_REMAINS;
                return NULL;
            }
            //fprintf(stderr, "\n++%c%c%c%c%c+++++\n", px[0], px[1], px[2], px[3], px[4]);
            //fprintf(stderr, "%s\n", px);
        }

        json = json_parse((char*)px, 0);

        free(pp);
        if (pm!=NULL) free(pm);
        ring->state = JBXL_NORMAL;
    }
    else {
        //print_message("--------- JBXL_INCOMPLETE\n");
        ring->state = JBXL_INCOMPLETE;
    }

    return json;
}


tJson*  ws_json_client(unsigned char* mesg, int cc)
{
    tJson* json = NULL;
    tJson* jtmp = NULL;

    static ringBuffer* cring = NULL;
    if (cring==NULL) cring = new_ringBuffer(BUFSZ2M);

    if ((mesg[0]!=0x81 && mesg[0]!=0x82) && cring->state==JBXL_NORMAL) return NULL;
    if ((mesg[0]==0x81 || mesg[0]==0x82) && cring->state==JBXL_INCOMPLETE) {
        clear_ringBuffer(cring);
    }
    put_ringBuffer(cring, mesg, cc);

    jtmp = ws_json_parse(cring);
    while (jtmp!=NULL || cring->state==JBXL_DATA_REMAINS) {
        if (jtmp!=NULL) json = join_json(json, &jtmp);
        jtmp = ws_json_parse(cring);
    }

    return json;
}

 
tJson*  ws_json_server(unsigned char* mesg, int cc)
{
    tJson* json = NULL;
    tJson* jtmp = NULL;

    static ringBuffer* sring = NULL;
    if (sring==NULL) sring = new_ringBuffer(BUFSZ2M);

    if ((mesg[0]!=0x81 && mesg[0]!=0x82) && sring->state==JBXL_NORMAL) return NULL;
    if ((mesg[0]==0x81 || mesg[0]==0x82) && sring->state==JBXL_INCOMPLETE) {
        clear_ringBuffer(sring);
    }
    put_ringBuffer(sring, mesg, cc);

    jtmp = ws_json_parse(sring);
    while (jtmp!=NULL || sring->state==JBXL_DATA_REMAINS) {
        if (jtmp!=NULL) json = join_json(json, &jtmp);
        jtmp = ws_json_parse(sring);
    }

    return json;
}



///////////////////////////////////////////////////////////////////////
// get information from HTTP

//
// 受信データから，存在するならば セッションID を取り出す．
// SESSION_ID_KEY はクライアント（Webブラウザ）からのリクエストの ヘッダ中に設定されている値．
// 要 free
//
char*  get_info_from_header(char* mesg, char* key)
{
    if (mesg==NULL || key==NULL) return NULL;
    //
    char* pl = get_line(mesg, 1);
    if (pl==NULL) return NULL;
    char* pp = strstr(pl, key);
    if (pp==NULL) return NULL;
    pp = pp + strlen(key);

    char* pt = pp;
    while(*pt!=' ' && *pt!='&' && *pt!='%' && *pt!='\0') pt++;

    *pt = '\0';
    char* value = dup_str(pp);
    free(pl);

    //print_message("get_info_from_header --> %s%s\n", key, value);
    return value;
}


//
// HTTPのPOST受信データ（mesg）のボディから コースIDとLTIのインスタンスID を取り出す．
// mesg は LTI コンシューマ（クライアント: Moodle）からのデータ．
// 要 free
//
// char* get_info_from_sessioninfo();
char*  get_info_from_body(char* mesg, char* key)
{
    if (mesg==NULL || key==NULL) return NULL;

    char* pp = mesg;
    if (ex_strcmp("POST ", (char*)mesg)){
        pp = strstr(mesg, "\r\n\r\n");    // Body
        if (pp==NULL) return NULL; 
    }
    //
    // Search in the Body
    pp = strstr(pp, key);
    if (pp==NULL) return NULL;
    pp = pp + strlen(key);
    //
    char* pt = pp; 
    while (*pt!='&' && *pt!='\0') pt++;

    char bkup = *pt;
    *pt = '\0';
    char* value = dup_str(pp);
    *pt = bkup;

    //print_message("get_info_from_body --> %s%s\n", key, value);
    return value;
}


//
// クライアント（Webブラウザ）のクッキーから コースIDとLTIのインスタンスID を取り出す．
// 要 free
//
char*  get_info_from_cookie(char* mesg, char* key)
{
    if (mesg==NULL || key==NULL) return NULL;

    char* pp = strstr(mesg, "Cookie:");
    if (pp==NULL) pp = strstr(mesg, "cookie:");
    if (pp==NULL) return NULL;

    char* pl = get_line(pp, 1);
    if (pl==NULL) return NULL;
    pp = strstr(pl, key);
    if (pp==NULL) return NULL;
    pp = pp + strlen(key);

    char* pt = pp; 
    while (*pt!=';' && *pt!='\0') pt++;

    *pt = '\0';
    char* value = dup_str(pp);
    free(pl);

    //print_message("get_info_from_cookie --> %s%s\n", key, value);
    return value;
}



///////////////////////////////////////////////////////////////////////
// tools

//
// JSON データのノード値の文字列を返す．
// "" または '' で囲まれている場合は，その内部のデータ（"", ''の中味）の返す．
// 要 free
//
/*
char*  get_string_from_json(tJson* json)
{
    if (json==NULL) return NULL;

    char* str = NULL;
    char* pp  = (char*)json->ldat.val.buf;

    if (pp!=NULL && json->ldat.lv!=JSON_VALUE_ARRAY) {
        if (*pp=='\"' || *pp=='\'') {
            char* pt = (char*)&(json->ldat.val.buf[json->ldat.val.vldsz-1]);
            if (*pp==*pt) {
                pp++;
                char bkup = *pt;
                *pt = '\0';
                str = dup_str(pp);
                *pt = bkup;
            }
        }
    }
    else {
        str = dup_str(pp);
    }

    return str;
}
*/



///////////////////////////////////////////////////////////////////////
// send data to Moodle Web Service

void  send_data_server(void)
{
    int sock = tcp_client_socket(ServerName, ServerPort);
    if (sock<0) {
        print_message("feplg_nbws: Connect to %s %d: ", ServerName, ServerPort);
        jbxl_fprint_state(stderr, sock);
        return;
    }
    SSL_CTX* ctx = NULL;
    SSL* ssl     = NULL;

    if (ServerTLS) {
        ctx = ssl_client_setup(NULL);
        ssl = ssl_client_socket(sock, ctx, OFF);
    }

    DEBUG_MODE print_tList(stderr, HTTP_Header);
    send_https_header(sock, ssl, HTTP_Header, ON);
    if (XmlRpc_Response) {
        char ans[RECVBUFSZ];
        ssl_tcp_recv(sock, ssl, ans, RECVBUFSZ-1);
        print_message("%s\n", ans);
    }

    if (ServerTLS) {
        if (ssl!=NULL) ssl_close(ssl);
        SSL_CTX_free(ctx);
    }
    close(sock);

    return;
}


void  setup_xmlrpc_params(void)
{
    char* p;
    char* s;
    char  b;

    if (ServerURL==NULL) return;

    p = ServerURL;
    s = p;
    while (*p!='%' && *p!='\0') p++;
    b  = *p; 
    *p = '\0';
    if (!strncmp("https", s, 5)) ServerTLS = TRUE;
    else                         ServerTLS = FALSE;
    *p = b;
    //if (*p=='%') p += 3;
    //while (*p=='%') p += 3;
    if      (*p=='%') p += 3;        // :
    else if (*p==':') p++;
    //
    if      (*p=='%') p += 3;        // /
    else if (*p=='/') p++;
    if      (*p=='%') p += 3;        // /
    else if (*p=='/') p++;

    freenull(ServerName);
    s = p;
    while (*p!='%' && *p!='\0') p++;
    b  = *p; 
    *p = '\0';
    ServerName = dup_str(s);
    *p = b;
    if (b=='%') p += 3;

    s = p;
    ServerPort = atoi(s);

    return;
}


void  init_xmlrpc_header(void)
{
    tList* pp = NULL;
    pp = HTTP_Header = add_tList_node_str(pp, HDLIST_FIRST_LINE_KEY, "");
    pp = HTTP_Host   = add_tList_node_str(pp, "Host", "");
    pp               = add_tList_node_str(pp, "Content-Type", "text/html");
    pp = HTTP_Length = add_tList_node_str(pp, "Content-Length", "");
    pp               = add_tList_node_str(pp, "Connection", "close");
    pp               = add_tList_node_str(pp, HDLIST_END_KEY, "");
    pp = HTTP_Data   = add_tList_node_str(pp, HDLIST_CONTENTS_KEY, "");

    return;
}


void  post_xml_server(struct ws_info* info)
{
    //print_message("start post_xml_server\n");
    setup_xmlrpc_params();
    if (ServerPath==NULL || ServerName==NULL) return;

    // data -> HTTP_Header, HTTP_Host
    char* path = ServerPath;
    if (*path=='%') path += 3;
    if (*path=='/') path++;
    if (path==NULL) return;

    char url[LMESG];
    snprintf(url, LMESG-1, "POST /%s%s?wstoken=%s HTTP/%s", path, XmlRpc_Path, ServerToken, XmlRpc_HTTPver);
    copy_s2Buffer(url, &(HTTP_Header->ldat.val));
    copy_s2Buffer(ServerName, &(HTTP_Host->ldat.val));
    //print_message("  post_xml_server url=%s\n", url);

    tXML* xml = NULL;
    xml = xml_rpc_add_member(xml, "host",     info->host, "");
    xml = xml_rpc_add_member(xml, "inst_id",  info->inst_id, "");
    xml = xml_rpc_add_member(xml, "lti_id",   info->lti_id, "");
    xml = xml_rpc_add_member(xml, "session",  info->session, "");
    xml = xml_rpc_add_member(xml, "message",  info->message, "");
    xml = xml_rpc_add_member(xml, "status",   info->status, "");
    xml = xml_rpc_add_member(xml, "username", info->username, "");
    xml = xml_rpc_add_member(xml, "cell_id",  info->cell_id, "");
    xml = xml_rpc_add_member(xml, "tags",     info->tags, "");
    xml = xml_rpc_add_member(xml, "date",     info->date, "");
    xml = xml_rpc_end_member(xml);

    // data -> HTTP_Length, HTTP_Data
    Buffer buf = xml_rpc_request_pack(XmlRpc_Service, xml);
    copy_i2Buffer((int)buf.vldsz, &(HTTP_Length->ldat.val));
    copy_Buffer(&buf, &(HTTP_Data->ldat.val));

    send_data_server();

    free_Buffer(&buf);
    del_xml(&xml);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// 全プロセスの共通初期化処理
//
int  init_main(int mode, tList* flist)
{
    //if (flist!=NULL) print_tTree(stderr, flist, "    ");

    //openlog("FEServer Jupyter WebScoket Plugin", LOG_PERROR|LOG_PID, LOG_AUTH);
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

    // config file
    XmlRpc_Path     = get_str_param_tList (flist, XMLRPC_PATH_KEY ,    XmlRpc_Path);
    XmlRpc_Service  = get_str_param_tList (flist, XMLRPC_SERVICE_KEY,  XmlRpc_Service);
    XmlRpc_HTTPver  = get_str_param_tList (flist, XMLRPC_HTTPVER_KEY,  XmlRpc_HTTPver);
    XmlRpc_Response = get_bool_param_tList(flist, XMLRPC_RESPONSE_KEY, XmlRpc_Response);

    init_xmlrpc_header();

    return  Logtype;
}



//
// 全プロセスの共通終了処理
//
int  term_main(void)
{
    del_tList(&HTTP_Header);

    freenull(XmlRpc_Path);
    freenull(XmlRpc_Service);
    freenull(XmlRpc_HTTPver);

    return 0;
}



//
// プロセス毎の初期化処理
//
int  init_process(int dummy, char* client)
{
    UNUSED(dummy);

    //EBUG_MODE print_message("接続許可・禁止の確認．\n");
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
    //
    return TRUE;
}



//
// プロセス毎の終了処理
//
int  term_process(int dummy)
{
    UNUSED(dummy);

    del_all_tList(&Allow_IPaddr);

    freenull(SessionInfo);
    freenull(ServerURL);
    freenull(ServerName);
    freenull(ServerPath);
    freenull(ServerToken);

    return TRUE;
}



//
//    Server -> Client (cofd -> sofd) 
//
int   fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);
    Buffer shd = init_Buffer();

    int http_res = 0;

    //print_message("SERVER +++++++++++++++++++++++++++++++++++ %d\n", getpid());
    //print_message("%s\n",mesg);

    // add cookie
    // Session Info を lms_sessioninfo の値として cookie に追加
    if (SessionInfo!=NULL && ex_strcmp("HTTP/", (char*)mesg)){
        http_res = 1;
        //
        Buffer buf = make_Buffer_bystr(mesg);
        tList* lhd = get_protocol_header_list(buf, ':', TRUE, TRUE);
        tList* chk = search_key_tList(lhd, HDLIST_CONTENTS_KEY, 1);
        //
        if (chk==NULL) {
            tList* cke = search_key_tList(lhd, "set-cookie", 1);
            if (cke!=NULL) {
                char cookie[LMESG];
                snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_SESSIONINFO_KEY, SessionInfo);
                cke  = add_protocol_header(cke, "set-cookie", cookie);
                snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_SERVERURL_KEY, ServerURL);
                cke  = add_protocol_header(cke, "set-cookie", cookie);
                snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_SERVERPATH_KEY, ServerPath);
                cke  = add_protocol_header(cke, "set-cookie", cookie);
                snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_RPCTOKEN_KEY, ServerToken);
                cke  = add_protocol_header(cke, "set-cookie", cookie);
                //
                shd  = restore_protocol_header(lhd, ": ", OFF, NULL);
                mesg = (char*)shd.buf;
                cc   = shd.vldsz;
            }
        }
        free_Buffer(&buf);
        del_tList(&lhd);
    }

    //////////////////////////////////////////////
    int ret = ssl_tcp_send(sofd, ssl, mesg, cc);
    //////////////////////////////////////////////

    if (shd.buf!=NULL) free_Buffer(&shd);

    //
    // Web Socket
    static char host[] = "server";

    tJson* temp = NULL;
    tJson* json = NULL;

//print_message("\n=SS================================================================================= %d\n", getpid());
//print_message("http_res = %d\n%s", http_res, mesg);
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    if (http_res==0) json = ws_json_server((unsigned char*)mesg, cc);
    if (json!=NULL && json->next!=NULL) {
print_message("\n+SS+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ %d\n", getpid());
print_json(stderr, json, JSON_INDENT_FORMAT);
        //
        tJson* sister = json->next;
        while (sister->esis!=NULL) sister = sister->esis;
        while (sister!=NULL) {
            struct ws_info info;
            memset(&info, 0, sizeof(struct ws_info));
            //
            char* type = get_string_from_json(search_key_json(sister, "msg_type", TRUE, 1));
            if (type!=NULL && ex_strcmp("execute_reply", type)) { 
                info.status = get_string_from_json(find_double_key_json(sister, "content", "status"));
                if (info.status!=NULL) {
                    temp = find_double_key_json(sister, "header", "username");
                    info.username = get_string_from_json(temp);
                    if (info.username!=NULL) {
                        info.date = get_string_from_json(find_key_sister_json(temp, "date"));
                        temp = find_double_key_json(sister, "parent_header", "session");
                        info.session = get_string_from_json(temp);
                        if (info.session!=NULL) {
                            info.message = get_string_from_json(find_key_sister_json(temp, "msg_id"));
                            info.host    = host;
                            post_xml_server(&info);
                            //
                            freenull(info.message);
                            freenull(info.session);
                        }
                        freenull(info.date);
                        freenull(info.username);
                    }
                    freenull(info.status);
                }
                freenull(type);
            }
            sister = sister->ysis;
        }
        //
        del_json(&json);
    }

    return ret;
}




//
//    Client -> Server (sofd -> cofd)
//
int   fe_client(int dummy1, int cofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

    //print_message("\nCLIENT +++++++++++++++++++++++++++++++++++ %d\n", getpid());
    //print_message("%s\n",mesg);

    //////////////////////////////////////////////
    cc = ssl_tcp_send(cofd, ssl, mesg, cc);
    //////////////////////////////////////////////

    static char fesvr[] = "fesvr";
    char num[10];

    static int content_length = 0;
    static Buffer recv_buffer;
    
    int http_com = 0;

    //
    // GET session_id と cookie の lms_sessionifo (course_id+%2C+lti_id) を関連付けて DB に登録．
    if (ex_strcmp("GET ", (char*)mesg)) {
        http_com = 1;
        content_length = 0;
        recv_buffer = init_Buffer();
        //
        char* sessionid = get_info_from_header(mesg, SESSION_ID_KEY); // URL パラメータから session_id を得る
        if (sessionid!=NULL) {
            char* ssninfo = get_info_from_cookie(mesg, LMS_SESSIONINFO_KEY);
            if (ssninfo!=NULL) {
                struct ws_info info;
                memset(&info, 0, sizeof(struct ws_info));
                //
                char* pt = ssninfo;
                while (*pt!='%' && *pt!='\0') pt++;
                if (*pt=='%') {
                    *pt = '\0';
                    pt = pt + 3;
                }
                info.host    = fesvr;
                info.inst_id = ssninfo;
                info.lti_id  = pt;
                info.session = sessionid;
                //
                freenull(ServerURL);
                freenull(ServerToken);
                ServerURL   = get_info_from_cookie(mesg, LMS_SERVERURL_KEY);
                ServerPath  = get_info_from_cookie(mesg, LMS_SERVERPATH_KEY);
                ServerToken = get_info_from_cookie(mesg, LMS_RPCTOKEN_KEY);

                post_xml_server(&info);
                //
                freenull(ssninfo);
                freenull(sessionid);
            }
        }
    }

    //
    else if (ex_strcmp("POST ", (char*)mesg)) {
        http_com = 2;
        content_length = 0;
        recv_buffer = init_Buffer();
        char* pp = strstr(mesg, "\r\n\r\n");
        char* pt = strstrcase(mesg, CONTENT_LENGTH);

        // content-length の取り出し
        if (pp!=NULL && pt!=NULL && (int)(pp-pt)>0) {
            pt += strlen(CONTENT_LENGTH);
            char* pr = pt;
            while (*pr!=0x0d && *pr!=0x0a && *pr!=0x00) pr++;
            int l = (int)(pr - pt);
            if (l<=10) {
                memcpy(&num, pt, l);
                num[l] = 0x00;
                content_length = atoi(num);
            }
        } 
 
        // Body部 の取り出し
        if (pp!=NULL && content_length>0) {
            pp += strlen("\r\n\r\n");
            int l = cc - (int)(pp - (char*)mesg); 
            recv_buffer = set_Buffer(pp, l);
        }
    }
    else if (ex_strcmp("PUT ", (char*)mesg)) {
        http_com = 3;
    }
    else if (ex_strcmp("DELETE ", (char*)mesg)) {
        http_com = 4;
    }
    else if (ex_strcmp("PATCH ", (char*)mesg)) {
        http_com = 5;
    }
    else if (content_length>0) {
        http_com = 2;
        cat_b2Buffer(mesg, &recv_buffer, cc);
    }

    // POST 受信完了
    if (content_length>0 && content_length==recv_buffer.vldsz) {
        mesg = (char*)recv_buffer.buf;
        //
        if (SessionInfo==NULL) {
            if (ex_strcmp("oauth_version", (char*)mesg)) {
                SessionInfo = get_info_from_body(mesg, LMS_SESSIONINFO_KEY);  
                ServerURL   = get_info_from_body(mesg, LMS_SERVERURL_KEY);  
                ServerPath  = get_info_from_body(mesg, LMS_SERVERPATH_KEY);  
                ServerToken = get_info_from_body(mesg, LMS_RPCTOKEN_KEY);  
            }
            // 
            if (SessionInfo==NULL && strstr(mesg, LMS_SESSIONINFO_KEY) != NULL) {
                SessionInfo = get_info_from_body(mesg, LMS_SESSIONINFO_KEY);  
                ServerURL   = get_info_from_body(mesg, LMS_SERVERURL_KEY);  
                ServerPath  = get_info_from_body(mesg, LMS_SERVERPATH_KEY);  
                ServerToken = get_info_from_body(mesg, LMS_RPCTOKEN_KEY);  
            }
        }
        content_length = 0;
        free_Buffer(&recv_buffer);
    }

    //
    // Web Socket
    static char host[]  = "client";

    tJson* temp = NULL;
    tJson* json = NULL;
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    if (http_com==0) json = ws_json_client((unsigned char*)mesg, cc);
    if (json!=NULL && json->next!=NULL) {
print_message("\n+CC+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ %d\n", getpid());
print_json(stderr, json, JSON_INDENT_FORMAT);
        //
        tJson* sister = json->next;
        while (sister->esis!=NULL) sister = sister->esis;
        while (sister!=NULL) {
            struct ws_info info;
            memset(&info, 0, sizeof(struct ws_info));
            //
            temp = find_double_key_json(sister, "metadata", "cellId");
            info.cell_id = get_string_from_json(temp);
            if (info.cell_id!=NULL) {
                info.tags = get_string_from_json(find_key_sister_json(temp, "tags"));
                temp = find_double_key_json(sister, "header", "session");
                info.session = get_string_from_json(temp);
                if (info.session!=NULL) {
                    info.date     = get_string_from_json(find_key_sister_json(temp, "date"));
                    info.message  = get_string_from_json(find_key_sister_json(temp, "msg_id"));
                    info.host     = host;
                    post_xml_server(&info);
                    //
                    freenull(info.message);
                    freenull(info.date);
                    freenull(info.session);
                }
                freenull(info.tags);
                freenull(info.cell_id);
            }
            sister = sister->ysis;
        }
        //
        del_json(&json);
    }

    //
    return cc;
}

