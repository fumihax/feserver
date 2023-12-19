v3.0.1

Usage... ./fesvr host_name[:port] -p port -m module_name [-u user_name] [-i] [-s] [-c] [--conf config_file] [--cert cert_file] [--key key_file] [-d]

-h  サーバFQDN + ポート番号．サーバのポート番号を省略した場合は，ローカルポートの番号と同じになる．必須
-p  ローカルポート番号．必須
-m  処理モジュール．必須
-u  実効ユーザ名
-i  デーモンモードを無効化．スーパデーモン経由で使用する場合に指定する
-d  デバッグモード
-s  サーバに対してSSL接続になる（fesrv はSSLクライアントとなる）．サーバ証明書の検証は行わない． 
-c  クライアントに対してSSL接続になる（fesrv はSSLサーバとなる）．
--conf  設定ファイル指定
--cert  -c を指定した場合のサーバ証明書：PEM形式．  デフォルトは /etc/pki/tls/certs/server.pem
--key   -c を指定した場合のサーバの秘密鍵：PEM形式．デフォルトは /etc/pki/tls/private/key.pem


・自己証明書作成のコマンド例
  # cd /etc/pki/tls
  # openssl req -new -newkey rsa:2048 -days 3650 -nodes -keyout private/key.pem -out server.csr
  # openssl x509 -in server.csr -days 3650 -req -signkey private/key.pem -out certs/server.pem

