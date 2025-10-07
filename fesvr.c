/*  
    Front End Server Core 
        
                by Fumi.Iseki '22 01/09   BSD License.
*/

#define  FESVR_VERSION      "3.1.2"


#include "fesvr.h"
#include "jbxl_state.h"


#define  FESERVER_TIMEOUT   900       // 15m

#define  DEFAULT_PID_FILE   "/var/run/fesvr.pid"
#define  DEFAULT_CERT_FILE  "/etc/pki/tls/certs/server.pem"
#define  DEFAULT_CHAIN_FILE NULL
#define  DEFAULT_KEY_FILE   "/etc/pki/tls/private/key.pem"


int    DaemonMode = ON;

int    Nofd, Sofd, Cofd;
int    Log_Type;
pid_t  RootPID;
char*  PIDFile = NULL;

SSL*   Sssl = NULL;
SSL*   Cssl = NULL;
int    ClientSSL = OFF;     // サーバ側（自身はクライアント）とのSSL 接続
int    ServerSSL = OFF;     // クライアント側（自身はサーバ）とのSSL 接続

char*  ClientIPaddr  = NULL;
char*  ClientName    = NULL;
unsigned char*  ClientIPaddr_num  = NULL;



int main(int argc, char** argv)
{
    int  version = OFF;
    int  i, sport=0, cport;
    socklen_t cdlen;
    //struct sockaddr_in cl_addr;
    struct sockaddr  cl_addr;
    struct sigaction sa;
    struct passwd*  pw;

    Buffer hostname;
    Buffer modulename;
    Buffer username;
    Buffer pidfile;
    Buffer certfile;
    Buffer chainfile;
    Buffer keyfile;
    Buffer configfile;

    // 引数処理
    hostname   = init_Buffer();
    modulename = init_Buffer();
    username   = init_Buffer();
    pidfile    = init_Buffer();
    certfile   = init_Buffer();
    chainfile  = init_Buffer();
    keyfile    = init_Buffer();
    configfile = init_Buffer();

    for (i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"-p")) {if (i!=argc-1) sport = atoi(argv[i+1]);}
        else if (!strcmp(argv[i],"-h")) {if (i!=argc-1) hostname   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-m")) {if (i!=argc-1) modulename = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-u")) {if (i!=argc-1) username   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-f")) {if (i!=argc-1) pidfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-d")) DebugMode  = ON;
        else if (!strcmp(argv[i],"-i")) DaemonMode = OFF;
        else if (!strcmp(argv[i],"--version")) version = ON;
        //
        else if (!strcmp(argv[i],"-s")) ClientSSL  = ON;
        else if (!strcmp(argv[i],"-c")) ServerSSL  = ON;
        else if (!strcmp(argv[i],"--cert"))   {if (i!=argc-1) certfile   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--chain"))  {if (i!=argc-1) chainfile  = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--key"))    {if (i!=argc-1) keyfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--conf"))   {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--config")) {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        //
        else if (*argv[i]=='-') print_message("unknown argument: %s\n", argv[i]);
    }
    if (version==ON) {
        printf("%s\n", FESVR_VERSION);
        exit(0);
    }
    if (hostname.buf==NULL || modulename.buf==NULL || sport==0) {
        print_message("Usage... %s -h host_name[:port] -p port -m module_path [-s] [-c] [-i] [-u user] [-f pid_file] [-d] \n", argv[0]);
        print_message("                 [--conf config_file]  [--cert cert_file] [--key key_file] [--version]\n");
        exit(1);
    }

    i = 0;
    while(hostname.buf[i]!='\0' && hostname.buf[i]!=':') i++;
    if (hostname.buf[i]==':') {
        cport = atoi((char*)&(hostname.buf[i+1]));
        hostname.buf[i] = '\0';
    }
    else cport = sport;

    if (pidfile.buf==NULL) copy_s2Buffer(DEFAULT_PID_FILE, &pidfile);
    PIDFile = (char*)pidfile.buf;

    // Config File
    tList* filelist = NULL;
    if (configfile.buf!=NULL) filelist = read_index_tList_file((char*)configfile.buf, ' ');
    //
    if (certfile.buf==NULL) {
        char* cert = get_str_param_tList(filelist, "Fesvr_Server_Cert", DEFAULT_CERT_FILE);
        if (cert!=NULL) {
            copy_s2Buffer(cert, &certfile);
            free(cert);
        }
    }
    if (chainfile.buf==NULL) {
        char* chain = get_str_param_tList(filelist, "Fesvr_Server_Chain", DEFAULT_CHAIN_FILE);
        if (chain!=NULL) {
            copy_s2Buffer(chain, &chainfile);
            free(chain);
        }
    }
    if (keyfile.buf==NULL) {
        char* key  = get_str_param_tList(filelist, "Fesvr_Private_Key", DEFAULT_KEY_FILE);
        if (key!=NULL) {
            copy_s2Buffer(key, &keyfile);
            free(key);
        }
    }

    //
    // モジュール & syslog の初期化
    DEBUG_MODE print_message("モジュールの初期化開始．\n");
    if (modulename.buf[0]!='/') ins_s2Buffer("./", &modulename);
    if (!load_module((char*)modulename.buf)) {
        print_message("モジュールの読み込み失敗 [%s]．\n", modulename.buf);
        exit(1);
    }
    //
    Log_Type = init_main(DebugMode, filelist);
    if (Log_Type<0) {
        print_message("モジュールの初期化失敗．\n");
        exit(1);
    }
    DEBUG_MODE print_message("モジュールの初期化完了．\n");

    // テスト接続
    //DEBUG_MODE print_message("サーバ応答確認開始．\n");
    //Cofd = tcp_client_socket((char*)hostname.buf, cport);
    //if (Cofd<0) {
    //    syslog(Log_Type, "tcp_client_socket() error: [%s]", strerror(errno));
    //    DEBUG_MODE print_message("サーバポートへのアクセス不能．\n");
    //    //exit(1);
    //}
    //close(Cofd);
    //DEBUG_MODE print_message("サーバ応答確認完了．\n");

    // シグナルハンドリング
    DEBUG_MODE print_message("シグナル処理定義．\n");
    sa.sa_handler = sig_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    #
    set_sigterm_child(NULL);            // Childプロセスの終了処理設定（デフォルト処理を行う）

    // PIDファイルの作成
    RootPID = getpid();
    if (pidfile.buf!=NULL) {
        FILE*  fp;
        fp = fopen((char*)pidfile.buf, "w");
        if (fp!=NULL) {
            fprintf(fp, "%d", (int)RootPID);
            fclose(fp);
        }
    }

    // 実効ユーザの変更
    if (username.buf!=NULL) {
        int err = -1;

        DEBUG_MODE print_message("実効ユーザの変更 [%s]．\n", username.buf);
        if (isdigit(username.buf[0]) || username.buf[0]=='-') {
            err = seteuid(atoi((char*)username.buf));
        }
        else {
            pw = getpwnam((char*)username.buf);
            if (pw!=NULL) err = seteuid(pw->pw_uid);
        }
        if (err==-1) {
            syslog(Log_Type, "Cannot change effective user (%s): [%s]", username.buf, strerror(errno));
            DEBUG_MODE print_message("実効ユーザ [%s] にチェンジできません．\n", username.buf);
        }
    }

    // socket open for client
    DEBUG_MODE print_message("クライアント接続用ポートオープン．\n");
    Nofd = tcp_server_socket(sport);
    if (Nofd<0) {
        syslog(Log_Type, "tcp_server_socket() error: [%s]", strerror(errno));
        print_message("クライアント接続用ポートのオープンエラー．\n");
        exit(1);
    }

    cdlen = sizeof(cl_addr);
    // for SSL/TLS
    SSL_CTX* server_ctx = NULL;
    SSL_CTX* client_ctx = NULL;
    if (ServerSSL==ON || ClientSSL==ON) {
        ssl_init();
        if (ServerSSL==ON) {
            server_ctx = ssl_server_setup((char*)certfile.buf, (char*)keyfile.buf, (char*)chainfile.buf);
            if (server_ctx==NULL) print_message("サーバ用CTX の作成エラー．(%s) (%s) (%s)\n", (char*)certfile.buf, (char*)keyfile.buf, (char*)chainfile.buf);
        }
        if (ClientSSL==ON) {
            client_ctx = ssl_client_setup(NULL);
            if (client_ctx==NULL) print_message("クライアント用CTX の作成エラー．\n");
        }
    }

    // main loop
    DEBUG_MODE print_message("メインループ開始．\n");
    if (DaemonMode==ON) {
        Loop {
            Sofd = accept_intr(Nofd, &cl_addr, &cdlen);
            if (Sofd<0) {
                syslog(Log_Type, "accept() error: [%s]", strerror(errno));
                print_message("クライアントからの接続失敗．\n");
                exit(1);
            }

            if (fork()==0) receipt((char*)hostname.buf, cport, cl_addr, server_ctx, client_ctx);
            close(Sofd);        // don't use socket_close() !
        }
    }
    else {
        Sofd = accept_intr(Nofd, &cl_addr, &cdlen);
        if (Sofd<0) {
            syslog(Log_Type, "accept() error: [%s]", strerror(errno));
            print_message("クライアントからの接続失敗．\n");
            exit(1);
        }
        receipt((char*)hostname.buf, cport, cl_addr, server_ctx, client_ctx);
        close(Sofd);
    }
    DEBUG_MODE print_message("メインループ終了．\n");

    //
    close(Nofd);
    Sofd = Nofd = 0;

    if (server_ctx!=NULL)  SSL_CTX_free(server_ctx);
    if (client_ctx!=NULL)  SSL_CTX_free(client_ctx);
    if (pidfile.buf!=NULL) remove((const char*)pidfile.buf);   

    free_Buffer(&hostname);
    free_Buffer(&modulename);
    free_Buffer(&username);
    free_Buffer(&pidfile);
    free_Buffer(&certfile);
    free_Buffer(&chainfile);
    free_Buffer(&keyfile);
    free_Buffer(&configfile);
    del_tList(&filelist);

    exit(0);
}


//
void  receipt(char* hostname, int cport, struct sockaddr addr, SSL_CTX* server_ctx, SSL_CTX* client_ctx)
{
    int    cc, cs, nd;
    fd_set mask;
    char msg[MAXBUFSZ];
    struct timeval timeout;

    DEBUG_MODE print_message("子プロセスの開始．（%d）\n", getpid());

    init_rand();

    struct sockaddr_in* addr_ptr = (struct sockaddr_in*)&addr;
    
    // モジュールの開始処理
    DEBUG_MODE print_message("モジュールの初期処理．\n");
    ClientIPaddr_num = get_ipaddr_num_ipv4(addr_ptr->sin_addr);
    ClientIPaddr     = get_ipaddr_ipv4(addr_ptr->sin_addr);
    ClientName       = get_hostname_bynum_ipv4(ClientIPaddr_num);
    syslog(Log_Type, "[%s] session start.\n", ClientIPaddr);

    if (!init_process(Sofd, ClientName)) {
        syslog(Log_Type, "module start error.");
        print_message("モジュールの初期処理の失敗．(%d)\n", getpid());
        exit(1);
    }
    DEBUG_MODE print_message("モジュールの初期処理完了．(%d)\n", getpid());

    Sssl = NULL;
    Cssl = NULL;

    // for Client SSL Connection
    if (ServerSSL==ON && server_ctx!=NULL) {
        Sssl = ssl_server_socket(Sofd, server_ctx);
        if (Sssl==NULL) {
            sleep(1);
            Sssl = ssl_server_socket(Sofd, server_ctx);
            if (Sssl==NULL) {
                DEBUG_MODE print_message("クライアント用SSLソケットの作成失敗．(%d)\n", getpid());
                exit(1);
            }
        }
        DEBUG_MODE print_message("クライアント用SSLソケットのオープン．(%d)\n", getpid());
    }

    // for Server Connection
    Cofd = tcp_client_socket(hostname, cport);
    if (Cofd<0) {
        syslog(Log_Type, "tcp_client_socket() error: [%s]", strerror(errno));
        print_message("サーバへの接続に失敗．(%d)\n", getpid());
        exit(1);
    }
    if (ClientSSL==ON && client_ctx!=NULL && Sssl!=NULL) {
        Cssl = ssl_client_socket(Cofd, client_ctx, OFF);
        if (Cssl==NULL) {
            DEBUG_MODE print_message("サーバへのSSL接続の失敗．(%d)\n", getpid());
        }
    }
 
    int range = Max(Sofd, Cofd) + 1;

    //
    timeout.tv_sec  = FESERVER_TIMEOUT;
    timeout.tv_usec = 0;
    FD_ZERO(&mask); 
    FD_SET(Sofd, &mask);
    //FD_SET(Cofd, &mask);
    nd = select(range, &mask, NULL, NULL, &timeout);

    DEBUG_MODE print_message("通信の中継処理開始．(%d)\n", getpid());
    while(nd>0) {// && (FD_ISSET(Cofd, &mask) || FD_ISSET(Sofd, &mask))) {
        // Client -> Server // fesrv はサーバ
        if (FD_ISSET(Sofd, &mask)) {
            cc = ssl_tcp_recv(Sofd, Sssl, msg, MAXBUFSZ);      // Client から受信
            if (cc>0) {
                cs = fe_client(Sofd, Cofd, Sssl, Cssl, msg, cc);     // Server へ転送
                if (cs<=0) {
                    if (cs<0) syslog(Log_Type, "error occurred in fe_client().");
                    break;
                }
            }
            else {
                if (cc<0) {
                    print_message("fesvr: C->S: ");
                    jbxl_fprint_state(stderr, cc);
                }
                break;      // cc==0
            }
        }

        timeout.tv_sec  = FESERVER_TIMEOUT;
        timeout.tv_usec = 0;
        FD_ZERO(&mask); 
        FD_SET(Sofd, &mask);
        FD_SET(Cofd, &mask);
        nd = select(range, &mask, NULL, NULL, &timeout);

        // Server -> Client // fesrv はクライアント
        if (FD_ISSET(Cofd, &mask)) {
            cc = ssl_tcp_recv(Cofd, Cssl, msg, MAXBUFSZ);      // Server から受信
            if (cc>0) {
                cs = fe_server(Cofd, Sofd, Cssl, Sssl, msg, cc);     // Client へ転送
                if (cs<=0) {
                    if (cs<0) syslog(Log_Type, "error occurred in fe_server().");
                    //break;
                }
            }
            else {
                if (cc<0) {
                    print_message("fesvr: S->C: ");
                    jbxl_fprint_state(stderr, cc);
                }
                break;      // cc==0
            }
        }

        timeout.tv_sec  = FESERVER_TIMEOUT;
        timeout.tv_usec = 0;
        FD_ZERO(&mask); 
        FD_SET(Sofd, &mask);
        FD_SET(Cofd, &mask);
        nd = select(range, &mask, NULL, NULL, &timeout);
    }
    DEBUG_MODE print_message("通信の中継処理終了．(%d)\n", getpid());

    ssl_close(Cssl);
    ssl_close(Sssl);
    close(Cofd);
    Cssl = Sssl = NULL;
    Cofd = 0;

    syslog(Log_Type, "[%s] session end.", ClientIPaddr);

    // モジュールの終了処理
    DEBUG_MODE print_message("モジュールの終了処理．(%d)\n", getpid());
    if (!term_process(Sofd)) {
        syslog(Log_Type, "module termination error.");
        print_message("モジュールの終了処理の失敗．(%d)\n", getpid());
        exit(1);
    }

    if (DaemonMode==ON) {       // child process の終了
        close(Sofd);
        DEBUG_MODE print_message("子プロセスの終了．(%d)\n", getpid());
        exit(0);
    }
    return;
}
 


//
// プログラムの終了
//
void  sig_term(int signal)
{
    UNUSED(signal);

    pid_t pid = getpid();
    if (pid==RootPID) {
        if (term_main()) DEBUG_MODE print_message("プログラムの終了処理の失敗．(%d)\n", getpid());
    }

    if (Cofd>0) { close(Cofd); Cofd = 0;}
    if (Sofd>0) { close(Sofd); Sofd = 0;}
    if (Nofd>0) { close(Nofd); Nofd = 0;}
    if (PIDFile!=NULL) remove(PIDFile);

    closelog(); // close syslog 
    //exit(signal);
    exit(0);
}



//
// child プロセスの終了
//
void  sig_child(int signal)
{
    pid_t pid = 0;
    int ret;

    UNUSED(signal);
    //DEBUG_MODE print_message("SIG_CHILD: signal = %d\n", signal);

    do {
        pid = waitpid(-1, &ret, WNOHANG);
    } while(pid>0);
}



int  load_module(char* mname)
{
    void* moduleh = NULL;

    moduleh = dlopen(mname, RTLD_LAZY);
    //moduleh = dlopen(mname, RTLD_NOW);
    if (moduleh!=NULL) {
        init_main    = load_function(moduleh, "init_main");
        term_main    = load_function(moduleh, "term_main");
        init_process = load_function(moduleh, "init_process");
        term_process = load_function(moduleh, "term_process");
        fe_server    = load_function(moduleh, "fe_server");
        fe_client    = load_function(moduleh, "fe_client");
        if (init_main==NULL || term_main == NULL  || init_process==NULL 
                            || term_process==NULL || fe_server == NULL  || fe_client==NULL) return FALSE;
    }
    else {
        print_message("外部モジュールの読み込み失敗 [%s]．\n", mname);
        print_message("%s\n", dlerror ());
        return FALSE;
    }

    return TRUE;
}



void*  load_function(void* mh, char* fname)
{
    void* func;

    func = dlsym(mh, fname);
    if (func==NULL) {
        print_message("共有ライブラリ中に，関数[%s] が見つかりません．\n", fname);
        return NULL;
    }

    return func;
}


