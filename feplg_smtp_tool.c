/* vi: set tabstop=4 nocindent noautoindent: */

/*  
    FESERVER SMTP TOOL
        
                by Fumi Iseki '05 09/21
*/



#include "feplg_smtp_tool.h"




/**
int  proc_mime_filename(FILE* fp, char* bndry, int mode)

    機能：ファイルから 添付ファイル名を探し出し，登録された拡張子の場合はモードにより
　　　　　通知，またはファイル名を書き換える．
          モードにより戻り値の意味が違うので注意．

    引数：fp     --  ファイルポインタ
        　bndry  --  "--" + MIMEの境界文字列
          mode   --  0 : 通知のみ
                     1 : ファイル名を書き換える．

    戻り値：mode==0 
                FALSE(0): 登録された拡張子に一致しない．
                TRUE(!0): 一致した．

            mode==1  書き換えたファイル名の数
                 0: 一つも一致しなかった．
                -1: 一致したものがあったが，ファイル名の書き換えに失敗した．
*/
int  proc_mime_filename(FILE* fp, char* bndry, int mode)
{
    int   cnt   = 0;
    int   namef = OFF;
    int   contf = OFF;
    int   encdf = MIME_ERR_ENCODE;
    char  buf [LBUF];
    char  name[LBUF];
    char* dec;
    Buffer  fname;

    fname = make_Buffer(LBUF);
    if (fname.buf==NULL) return 0;

    fseek(fp, 0, SEEK_SET);
    memset(name, 0, LBUF);
    fgets(buf, LBUF, fp);
    
    // ファイルから MIMEヘッダを抽出    
    while (!feof(fp)) {
        chomp(buf);                    // CR, LF を削除
        if (!strcmp(buf, bndry)) {  // mime boundary を見つけた
            fgets(buf, LBUF, fp);

            while(!feof(fp)) {

                if (namef==ON) {    // ファイル名処理中
                    if ((buf[0]!=' ' && buf[0]!=CHAR_TAB) || buf[0]=='\0') {        // 前行の処理
                        // fname.buf: 処理中のファイル名（未デコード）
                        // name:      ファイル名の最後の部分（書き換える部分）（未デコード）

                        dec = decode_mime_string((char*)fname.buf);
                        DEBUG_MODE print_message("filename = [%s]\n", dec);

                        if (is_bad_extention(dec)) {
                            if (mode==0) {
                                free(dec);
                                free_Buffer(&fname);
                                return TRUE;
                            }

                            if (change_filename_end(name, encdf, '-')) {
                                int ret;
                                fseek(fp, -strlen(buf)-strlen(name), SEEK_CUR);
                                ret = fwrite(name, strlen(name), 1, fp);
                                if (ret==0) fseek(fp, strlen(buf)+strlen(name), SEEK_CUR);
                                else        fseek(fp, strlen(buf), SEEK_CUR);
                                fflush(fp);    
                                cnt++;
                            }
                            else {
                                free(dec);
                                free_Buffer(&fname);
                                return -1;
                            }
                        }

                        contf = OFF;
                        namef = OFF;
                        encdf = MIME_ERR_ENCODE;
                        freeNull(dec);
                        clear_Buffer(&fname);
                    }

                    else {        // ファイル名は複数の行に継続中
                        int  i = 0;
                        while(buf[i]!='\0' && (buf[i]==' ' || buf[i]==CHAR_TAB)) i++;
                        cat_s2Buffer(&(buf[i]), &fname);    
                    }
                }

                // ファイル名探索中
                // CONTENT 行
                if (!strncasecmp(buf, MIME_CONTENT_LINE, strlen(MIME_CONTENT_LINE))) {
                    contf = ON;
                    namef = OFF;
                    encdf = MIME_ERR_ENCODE;
                }

                // NAME= 行
                else if ((strstrcase(buf, MIME_NAMEEQ_LINE)!=NULL) ||
                         (strstrcase(buf, MIME_FILENAMESTAR_LINE)!=NULL)) {
                    if (contf==ON && namef==OFF) {
                        int  i = 0;
                        while(buf[i]!='\0' && buf[i]!='=') i++;
                        if (buf[i]=='=') i++;
                        copy_s2Buffer(&(buf[i]), &fname);
                        encdf = get_mime_enckind(buf);
                        namef = ON; 
                    }
                }

                memcpy(name, buf, LBUF);
                fgets(buf, LBUF, fp);
                if (name[0]=='\0') break;
            } 
            contf = namef = OFF;
        }
        else {
            fgets(buf, LBUF, fp);
        }
    }

    free_Buffer(&fname);
    return cnt;
}



/**
int   proc_mime_filenameffn(char* fname, char* bndry, int mode)

    機能：ファイルから 添付ファイル名を探し出し，登録された拡張子の場合はモードにより
　　　　　通知，またはファイル名を書き換える．
          モードにより戻り値の意味が違うので注意．

    引数：fname  --  ファイル名
        　bndry  --  "--" + MIMEの境界文字列
          mode   --  0 : 通知のみ
                     1 : ファイル名を書き換える．

    戻り値：mode==0 
                FALSE: 登録された拡張子に一致しない．またはファイルのオープンに失敗．
                TRUE:  一致した．

            mode==1  書き換えたファイル名の数
                0 : 一つも一致しなかった．
                -1: 一致したものがあったが，ファイル名の書き換えに失敗した．
                -2: ファイルのオープンに失敗．
*/
int   proc_mime_filenameffn(char* fname, char* bndry, int mode)
{
    int   cnt;
    FILE* fp;

    fp = fopen(fname, "r+");
    if (fp==NULL) {
        if (mode==0) return FALSE;
        else          return -2;
    }

    cnt = proc_mime_filename(fp, bndry, mode);
    fclose(fp);

    return cnt;
}



/**
char*  check_extents(tList* lp)

    機能：ファイル名の格納されたリストから，拡張子が BADEXTENTIONS に一致
          するものを探し，そのファイル名を返す．

    引数：キー部にファイル名の入ったリスト

    戻り値：最初に一致したファイル名．
            NULL  一致しなかった．    
*/
char*  check_extents(tList* lp)
{
    char* buf;
    char* ret;
    int   len;

    while (lp!=NULL) {
        buf = (char*)(lp->ldat.key.buf);
        if (is_bad_extention(buf)) {
            len = (int)strlen(buf);
            ret = (char*)malloc(len+1);
            if (ret==NULL) return NULL;
            memset(ret, 0, len+1);
            strncpy(ret, buf, len);
            return ret;
        }
        lp = lp->next;
    }

    return NULL;
}



/**
int  is_bad_extention(char* fname)

    機能：ファイル名のが BADEXTENTIONS に一致するかどうかチェックする．

    引数：ファイル名

    戻り値：FALSE   一致しない．
            TRUE    一致する．

*/
int  is_bad_extention(char* fname)
{
    int   i, len, ret=FALSE;
    char* str;

    if (fname==NULL) return FALSE;
    len = strlen(fname);
    if (len==0) return FALSE;

    str = (char*)malloc(len+2);
    if (str==NULL) return FALSE;

    str[len+1] = '\0';
    str[len]   = '.';

    i = len - 1;
    while (i>=0) {
        if (fname[i]=='.') break;
        if (fname[i]>='a' && fname[i]<='z') str[i] = fname[i] + 'A' - 'a';
        else                                str[i] = fname[i];
        i--;
    }
    if (i>=0) {
        str[i] = fname[i];
        if (strstr(BADEXTENTIONS, &(str[i]))!=NULL) ret = TRUE;
    }
    
    free(str);
    return ret;
}



/**
void  print_mailaddr_in_list(tList* lp)

    機能：リスト中のキー部を表示する．

*/
void  print_mailaddr_in_list(tList* lp)
{
    while (lp!=NULL) {
        print_message("<%s>\n", lp->ldat.key.buf);
        lp = lp->next;
    }
    return;
}



/**
int   change_filename_end(char* buf, char cc)

    機能：ファイル名の最後を cc で置き換える．

    引数：buf -- エンコードされた（されていなかもしれない）ファイル名
        　cc  -- 置き換える文字

*/
int   change_filename_end(char* buf, int encd, char cc)
{
    int   i, len, kind, codef=OFF;
    char* pp;
    
    if (buf==NULL) return FALSE;

    if (encd>0) kind = encd;
    else        kind = get_mime_enckind(buf);
    if (kind==MIME_ERR_ENCODE) return FALSE;

    len = strlen(buf);
    i = len - 1;
    while(i>=0) {
        if (buf[i]!='"' && buf[i]!=CHAR_CR && buf[i]!=CHAR_LF) break;
        i--;
    }
    if (i<0) return FALSE;

    if (kind==MIME_BASE64_ENCODE) {
        if (i>=1) {
            if (buf[i-1]=='?' && buf[i]=='=') {                    // base64
                if (i>=6) {
                    char  enc[5];
                    char* dec;
                    char* ret;
                    int sz;

                    memcpy(enc, &(buf[i-5]), 4);
                    enc[4] = '\0';
                    dec = (char*)decode_base64((unsigned char*)enc, &sz);    
                    if (dec==NULL) return FALSE;
                    if (strlen(dec)==0 || sz<1) {
                        free(dec);
                        return FALSE;
                    }

                    dec[sz-1] = cc;
                    ret = (char*)encode_base64((unsigned char*)dec, sz);
                    free(dec);
                    if (ret==NULL) return FALSE;
                    if (strlen(ret)!=4) {
                        free(ret);
                        return FALSE;
                    }

                    memcpy(&(buf[i-5]), ret, 4);
                    free(ret);
                    codef = ON;
                }
            }
        }
    }

    else if (kind==MIME_QUTDPRNTBL_ENCODE) {
        if (i>=1) {
            if (buf[i-1]=='?' && buf[i]=='=') {                    // quoted printable
                if (i>=4) {
                    if (buf[i-4]=='=') {
                        pp = (char*)encode_hex(cc);
                        if (pp==NULL) return FALSE;
                        buf[i-3] = pp[0];
                        buf[i-2] = pp[1];
                        free(pp);
                        codef = ON;
                    }
                }
                if (i>=2 && codef==OFF) {
                    buf[i-2] = cc;
                    codef = ON;
                }
            }
        }
    }

    else if (kind==MIME_URL_ENCODE) {
        if (i>=2) {
            if (buf[i-2]=='%') {
                pp = (char*)encode_hex(cc);
                if (pp==NULL) return FALSE;
                buf[i-1] = pp[0];
                buf[i]   = pp[1];
                free(pp);
                codef = ON;
            }
        }
    }

    if (codef==OFF) {
        buf[i] = cc;
        codef = ON;
    }

    if (codef==OFF)            return FALSE;
    if (len!=(int)strlen(buf)) return FALSE;
    return TRUE;
}

