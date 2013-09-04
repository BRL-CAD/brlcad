/*
    Popt Library Test Program Number Too 
    
    --> "a real world test of popt bugs" <--

    Copyright (C) 1999 US Interactive, Inc.

    This program can be used under the GPL or LGPL at your
    whim as long as this Copyright remains attached.  
*/

#include "system.h"

char *PathnameOfKeyFile		= NULL;
char *PathnameOfOfferFile	= NULL;

char *txHost			= NULL;
int   txSslPort			= 443;
int   txStoreId			= 0;

char *contentProtocol		= NULL;
char *contentHost		= NULL;
int   contentPort		= 80;
char *contentPath		= NULL;

char *dbPassword		= NULL;
char *dbUserName		= NULL;

char *rcfile = "createuser-defaults";
char *username=NULL;

char *password			= NULL;
char *firstname			= NULL;
char *lastname			= NULL;
char *addr1			= NULL;
char *addr2			= NULL;
char *city			= NULL;
char *state			= NULL;
char *postal			= NULL;
char *country			= NULL;

char *email			= NULL;

char *dayphone			= NULL;
char *fax			= NULL;


int 
main(int argc, const char ** argv) {

    poptContext optCon;   /* context for parsing command-line options */
    struct poptOption userOptionsTable[] = {
        { "first", 'f', POPT_ARG_STRING, &firstname, 0,
            "user's first name", "first" },
        { "last", 'l', POPT_ARG_STRING, &lastname, 0,
            "user's last name", "last" },
        { "username", 'u', POPT_ARG_STRING, &username, 0,
            "system user name", "user" },
        { "password", 'p', POPT_ARG_STRING, &password, 0,
            "system password name", "password" },
        { "addr1", '1', POPT_ARG_STRING, &addr1, 0,
            "line 1 of address", "addr1" },
        { "addr2", '2', POPT_ARG_STRING, &addr2, 0,
            "line 2 of address", "addr2" },
        { "city", 'c', POPT_ARG_STRING, &city, 0,
            "city", "city" },
        { "state", 's', POPT_ARG_STRING, &state, 0,
            "state or province", "state" },
        { "postal", 'P', POPT_ARG_STRING, &postal, 0,
            "postal or zip code", "postal" },
        { "zip", 'z', POPT_ARG_STRING, &postal, 0,
            "postal or zip code", "postal" },
        { "country", 'C', POPT_ARG_STRING, &country, 0,
            "two letter ISO country code", "country" },
        { "email", 'e', POPT_ARG_STRING, &email, 0,
            "user's email address", "email" },
        { "dayphone", 'd', POPT_ARG_STRING, &dayphone, 0,
            "day time phone number", "dayphone" },
        { "fax", 'F', POPT_ARG_STRING, &fax, 0,
            "fax number", "fax" },
        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };
    struct poptOption transactOptionsTable[] = {
        { "keyfile", '\0', POPT_ARG_STRING, &PathnameOfKeyFile, 0,
            "transact offer key file (flat_O.kf)", "key-file" },
        { "offerfile", '\0', POPT_ARG_STRING, &PathnameOfOfferFile, 0,
            "offer template file (osl.ofr)", "offer-file" },
        { "storeid", '\0', POPT_ARG_INT, &txStoreId, 0,
            "store id", "store-id" },
        { "rcfile", '\0', POPT_ARG_STRING, &rcfile, 0,
            "default command line options (in popt format)", "rcfile" },
        { "txhost", '\0', POPT_ARG_STRING, &txHost, 0,
            "transact host", "transact-host" },
        { "txsslport", '\0', POPT_ARG_INT, &txSslPort, 0,
            "transact server ssl port ", "transact ssl port" },
        { "cnhost", '\0', POPT_ARG_STRING, &contentHost, 0,
            "content host", "content-host" },
        { "cnpath", '\0', POPT_ARG_STRING, &contentPath, 0,
            "content url path", "content-path" },
        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };

    struct poptOption databaseOptionsTable[] = {
        { "dbpassword", '\0', POPT_ARG_STRING, &dbPassword, 0,
            "Database password", "DB password" },
        { "dbusername", '\0', POPT_ARG_STRING, &dbUserName, 0,
            "Database user name", "DB UserName" },
        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };

    struct poptOption optionsTable[] = {
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE,  NULL, 0,
            "Transact Options (not all will apply)", NULL },
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE,  NULL, 0,
            "Transact Database Names", NULL },
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE,  NULL, 0,
            "User Fields", NULL },
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };

    optionsTable[0].arg = transactOptionsTable;
    optionsTable[1].arg = databaseOptionsTable;
    optionsTable[2].arg = userOptionsTable;

#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    optCon = poptGetContext("createuser", argc, argv, optionsTable, 0);
    poptReadConfigFile(optCon, rcfile );

    /* although there are no options to be parsed, check for --help */
    poptGetNextOpt(optCon);

    optCon = poptFreeContext(optCon);

    printf( "dbusername %s\tdbpassword %s\n"
            "txhost %s\ttxsslport %d\ttxstoreid %d\tpathofkeyfile %s\n"
	    "username %s\tpassword %s\tfirstname %s\tlastname %s\n"
	    "addr1 %s\taddr2 %s\tcity %s\tstate %s\tpostal %s\n"
	    "country %s\temail %s\tdayphone %s\tfax %s\n",
        dbUserName, dbPassword,
        txHost, txSslPort, txStoreId, PathnameOfKeyFile,
        username, password, firstname, lastname,
        addr1,addr2, city, state, postal,
        country, email, dayphone, fax);
    return 0;
}
