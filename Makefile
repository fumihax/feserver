# vi: set tabstop=4 noautoindent:


VERSION=2.7.0

#
#
SUBJ = fesvr feplg_nop.so feplg_smtp.so print_smtpacs 
SUBJ += feplg_nbws.so feplg_asn1.so feplg_xxx.so
#
all: $(SUBJ)

#
CC   = gcc
AR   = ar
TH   = touch

LIB_DIR = ../JunkBox_Lib
LIB_BSC_DIR = $(LIB_DIR)/Lib
LIB_EXT_DIR = $(LIB_DIR)/xLib

LIB_BSC = $(LIB_BSC_DIR)/libbasic.a
LIB_EXT = $(LIB_EXT_DIR)/libextend.a

#DEBUG = -DEBUG

CFLAGS  = -fPIC -W -Wall -DHAVE_CONFIG_H -DENABLE_SSL -I/usr/include -I$(LIB_DIR) -I$(LIB_BSC_DIR) -I$(LIB_EXT_DIR) $(DEBUG) 

SLIB = -L$(LIB_BSC_DIR) -L/usr/local/ssl/lib -lbasic -lssl -lm -lcrypto -lcrypt
ELIB = -L$(LIB_EXT_DIR) -lextend
#
#
#

.h.c:
	$(TH) $@


.c.o:
	$(CC) $< $(CFLAGS) -c -O2 


#
#
#
#
fesvr: fesvr.o  $(LIB_BSC) $(LIB_EXT)
	$(CC) $(@).o -ldl $(ELIB) $(SLIB) -O2 -o $@ 

feplg_smtp.so: feplg_smtp.o feplg_smtp_tool.o $(LIB_BSC) $(LIB_EXT)
	$(CC) feplg_smtp.o feplg_smtp_tool.o -shared $(ELIB) $(SLIB) -O2 -o $@ 

feplg_nop.so: feplg_nop.o $(LIB_BSC) $(LIB_EXT)
	$(CC) feplg_nop.o -shared $(ELIB) $(SLIB) -O2 -o $@ 

print_smtpacs: print_smtpacs.o feplg_smtp_tool.o $(LIB_BSC) $(LIB_EXT)
	$(CC) $(@).o feplg_smtp_tool.o $(ELIB) $(SLIB) -O2 -o $@ 

feplg_ldap.so: feplg_ldap.o $(LIB_BSC) $(LIB_EXT)
	$(CC) feplg_ldap.o -shared $(ELIB) $(SLIB) -O2 -o $@ 

feplg_asn1.so: feplg_asn1.o $(LIB_BSC) $(LIB_EXT)
	$(CC) feplg_asn1.o -shared $(ELIB) $(SLIB) -O2 -o $@ 

feplg_nbws.so: feplg_nbws.o $(LIB_BSC) $(LIB_EXT)
	$(CC) feplg_nbws.o -shared $(ELIB) $(SLIB) -O2 -o $@ 

feplg_xxx.so: feplg_xxx.o $(LIB_BSC) $(LIB_EXT)
	$(CC) feplg_xxx.o -shared $(ELIB) $(SLIB) -O2 -o $@ 


$(LIB_BSC):
	(cd $(LIB_BSC_DIR) && make)

$(LIB_EXT):
	(cd $(LIB_EXT_DIR) && make)



install: fesvr feplg_nop.so feplg_smtp.so print_smtpacs
	install -m 0755 fesvr /usr/local/bin
	install -m 0755 feplg_smtp.so /usr/local/bin
	install -m 0755 feplg_nop.so /usr/local/bin
	install -m 0755 print_smtpacs /usr/local/bin
	mkdir -p /usr/local/etc/feserver/smtp
	mkdir -p /usr/local/etc/feserver/nop
	chmod 0750 /usr/local/etc/feserver/smtp
	chmod 0750 /usr/local/etc/feserver/nop
	install -m 0740 conf/rwmessage.text /usr/local/etc/feserver/smtp
	install -m 0740 conf/vrmessage.text /usr/local/etc/feserver/smtp
	mkdir -p /var/feserver/smtp
	mkdir -p /var/feserver/nop
	chmod 0755 /var/feserver
	chown nobody /var/feserver/smtp
	chown nobody /var/feserver/nop
	chmod 0700 /var/feserver/smtp
	chmod 0700 /var/feserver/nop
	[ -f /etc/init.d/feplg_smtp ] || install -m 0755 conf/feplg_smtp /etc/init.d
	[ -f /etc/init.d/feplg_nop ]  || install -m 0755 conf/feplg_nop /etc/init.d


clean:
	rm -f *.o $(SUBJ) 


tgz:
	make clean
	(cd .. && tar zcf Dist/feserver-${VERSION}.tgz feserver)

