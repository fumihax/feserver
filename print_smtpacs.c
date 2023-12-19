/* vi: set tabstop=4 nocindent noautoindent: */

/*  
    Check Access File
        
                by Fumi Iseki '05 09/21
*/



#include "feplg_smtp.h"




int  main()
{
    tList* lp;

    DEBUG_MODE print_message("\n");
    lp = read_ipaddr_file(ALLOW_FILE);
    if (lp!=NULL) {
        DEBUG_MODE print_message("アクセス許可ファイルの読み込み完了．\n\n");
        print_address_in_list(stderr, lp);
        del_all_tList(&lp);
    }
    else {
        lp = read_ipaddr_file(DENY_FILE);
        if (lp!=NULL) {
            DEBUG_MODE print_message("アクセス禁止ファイルの読み込み完了．\n\n");
            print_address_in_list(stderr, lp);
            del_all_tList(&lp);
        }
    }

    lp = read_ipaddr_file(LOCAL_IP_FL);
    if (lp!=NULL) {
        DEBUG_MODE print_message("ローカルIPグループファイルの読み込み完了．\n");
        print_address_in_list(stderr, lp);
        del_all_tList(&lp);
    }
    else {
        DEBUG_MODE {
            unsigned char* n = get_myipaddr_num_ipv4();
            print_message("ローカルIPグループファイルの読み込み失敗．デフォルト設定を使用します．\n");
            print_message("%d.%d.%d.%d/%d.%d.%d.%d\n",n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);
        }
    }
    DEBUG_MODE print_message("\n");


    lp = read_tList_file(MY_DEST_FL, 1);
    if (lp!=NULL) {
        DEBUG_MODE print_message("配送許可ファイルの読み込み完了．\n");
        print_mailaddr_in_list(lp);
        del_all_tList(&lp);
    }
    else {
        DEBUG_MODE print_message("配送許可ファイの読み込み失敗．配送許可ファイルは必須です．\n");
    }
    DEBUG_MODE print_message("\n");

    return 0;
}




