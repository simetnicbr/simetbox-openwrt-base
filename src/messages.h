/** ----------------------------------------
 * @file  messages.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 10, 2011
 *------------------------------------------*/

#ifndef MESSAGES_H_
#define MESSAGES_H_
/**
 * @brief  A message content to be sended.
 * Folows the format FIELD=CONTENT
 * */
struct message_st {
    int type; /**< meta type */
    const char *field; /**< left side of message*/
    char *content; /**< right side of message*/
};
typedef struct message_st message_t;

/*GENERAL*/
__attribute__((unused))    static message_t M_SIMET_SERVER_REQUEST_NOW = {1, "SIMET_SERVER_REQUEST", "NOW"};
__attribute__((unused))    static message_t M_REQUEST_SERVERLOCALTIME = {2, "SERVERLOCALTIME", "NOW"};
__attribute__((unused))    static message_t M_DYNAMICRESULTS_YES = {3, "DYNAMICRESULTS", "YES"};
__attribute__((unused))    static message_t M_REQUEST_CLIENT_IPADDRESS = {4, "CLIENT", "IPADDRESS"};
__attribute__((unused))    static message_t M_REQUEST_SERVERTIME = {9, "SERVERTIME", "NOW"};
__attribute__((unused))    static message_t M_SIMET_SERVER_ACK_OK = {7, "SIMET_SERVER_ACK", "OK"};
__attribute__((unused))   static message_t M_TILDE = {6, "~", ""};
__attribute__((unused))   static message_t M_SERVERTIME = {9, "SERVERTIME", NULL};
__attribute__((unused))   static message_t M_SERVERLOCALTIME = {10, "SERVERLOCALTIME", NULL};
__attribute__((unused))   static message_t M_CLIENTIPADDRESS = {11, "CLIENTIPADDRESS", NULL};
__attribute__((unused))   static message_t M_YOURCITYSTATE = {12, "YOUR_CITY_STATE", NULL};
__attribute__((unused))   static message_t M_PTTLOCATION = {13, "PTTLOCATION", NULL};
__attribute__((unused))   static message_t M_ENDTESTS_OK = {13, "ENDTEST", "OK"};

/*RTT*/
__attribute__((unused))   static message_t M_RTT_OK = {14, "RTT", "OK"};
__attribute__((unused))   static message_t M_REQUEST_RTT_START = {10, "RTT", "START"};

/*TCP*/
__attribute__((unused))   static message_t M_TCPUPLOADTIME_OK = {15, "TCPUPLOADTIME", "OK"};
__attribute__((unused))   static message_t M_TCPUPLOADINTERVAL_OK = {16, "TCPUPLOADINTERVAL", "OK"};
__attribute__((unused))   static message_t M_TCPUPLOADPACKAGESIZE_OK = {17, "TCPUPLOADPACKAGESIZE", "OK"};
__attribute__((unused))   static message_t M_TCPDOWNLOADTIME_OK = {18, "TCPDOWNLOADTIME", "OK"};
__attribute__((unused))   static message_t M_TCPDOWNLOADINTERVAL_OK = {19, "TCPDOWNLOADINTERVAL", "OK"};
__attribute__((unused))   static message_t M_TCPDOWNLOADPACKAGESIZE_OK = {20, "TCPDOWNLOADPACKAGESIZE", "OK"};
__attribute__((unused))   static message_t M_TCPUPLOADINTERVAL = {21, "TCPUPLOADINTERVAL", "500"};
__attribute__((unused))   static message_t M_TCPUPLOADPACKAGESIZE = {22, "TCPUPLOADPACKAGESIZE", "1400"};
__attribute__((unused))   static message_t M_TCPDOWNLOADINTERVAL = {24, "TCPDOWNLOADINTERVAL", "500"};
__attribute__((unused))   static message_t M_TCPDOWNLOADPACKAGESIZE = {25, "TCPDOWNLOADPACKAGESIZE", "1400"};
__attribute__((unused))   static message_t M_TCPSOCKET_OK = {26, "TCPSOCKET", "OK"};
__attribute__((unused))   static message_t M_TCPDOWNLOAD_OK = {27, "TCPDOWNLOAD", "OK"};
__attribute__((unused))   static message_t M_TCPDOWNLOADRESULT_OK = {28, "TCPDOWNLOADRESULT", "OK"};
__attribute__((unused))   static message_t M_TCP_DOWNLOAD_START = {50, "TCPDOWNLOAD", "START"};
__attribute__((unused))   static message_t M_TCP_UPLOAD_OK = {51, "TCPUPLOAD", "OK"};
__attribute__((unused))   static message_t M_TCP_UPLOAD_START = {52, "TCPUPLOAD", "START"};
__attribute__((unused))   static message_t M_TCPUPLOADRESULT_SENDRESULTS = {11, "TCPUPLOADRESULT", "SENDRESULTS"};
__attribute__((unused))   static message_t M_UPLOADBANDWIDTH = {12, "UPLOADBANDWIDTH", NULL};
__attribute__((unused))   static message_t M_TCPUPLOADRESULT_DONE = {13, "TCPUPLOADRESULT", "DONE"};

/* UDP*/
__attribute__((unused))   static message_t M_MAXRATE = {99, "MAXRATE", NULL};
__attribute__((unused))   static message_t M_UDPPORT_OK = {100, "UDPPORT", "OK"};
/*******download*/
__attribute__((unused))   static message_t M_UDPDOWNLOADTIME = {101, "UDPDOWNLOADTIME", "11"};
__attribute__((unused))   static message_t M_UDPDOWNLOADTIME_OK = {102, "UDPDOWNLOADTIME", "OK"};
__attribute__((unused))   static message_t M_UDPDOWNLOADINTERVAL = {103, "UDPDOWNLOADINTERVAL", "500"};
__attribute__((unused))   static message_t M_UDPDOWNLOADINTERVAL_OK = {104, "UDPDOWNLOADINTERVAL", "OK"};
__attribute__((unused))   static message_t M_UDPDOWNLOAD_START = {105, "UDPDOWNLOAD", "START"};
__attribute__((unused))   static message_t M_UDPDOWNLOADPACKAGESIZE_OK = {107, "UDPDOWNLOADPACKAGESIZE", "OK"};
__attribute__((unused))   static message_t M_UDPDOWNLOAD_OK = {108, "UDPDOWNLOAD", "OK"};
__attribute__((unused))   static message_t M_UDPDOWNLOADSENDRATE_NOW = {109, "UDPDOWNLOADSENDRATE", "NOW"};
__attribute__((unused))   static message_t M_UDPDOWNLOADSENDRATE  = {110, "UDPDOWNLOADSENDRATE", NULL };
__attribute__((unused))   static message_t M_UDPDOWNLOADSENDRATE_DONE = {111, "UDPDOWNLOADSENDRATE", "DONE"};
/*******upload*/
__attribute__((unused))   static message_t M_UDPUPLOADTIME = {110, "UDPUPLOADTIME", "11"};
__attribute__((unused))   static message_t M_UDPUPLOADTIME_OK = {112, "UDPUPLOADTIME", "OK"};
__attribute__((unused))   static message_t M_UDPUPLOADINTERVAL = {113, "UDPUPLOADINTERVAL", "500"};
__attribute__((unused))   static message_t M_UDPUPLOADINTERVAL_OK = {114, "UDPUPLOADINTERVAL", "OK"};
__attribute__((unused))   static message_t M_UDPUPLOAD_START = {115, "UDPUPLOAD", "START"};
__attribute__((unused))   static message_t M_UDPUPLOADPACKAGESIZE_OK = {117, "UDPUPLOADPACKAGESIZE", "OK"};
__attribute__((unused))   static message_t M_UDPUPLOAD_OK = {118, "UDPUPLOAD", "OK"};
__attribute__((unused))   static message_t M_UDPUPLOADSENDRATE_NOW = {119, "UDPUPLOADSENDRATE", "NOW"};
__attribute__((unused))   static message_t M_UDPUPLOADSENDRATE  = {120, "UDPUPLOADSENDRATE", NULL };
__attribute__((unused))   static message_t M_UDPUPLOADSENDRATE_DONE = {121, "UDPUPLOADSENDRATE", "DONE"};
__attribute__((unused))   static message_t M_UDPUPLOADRESULT_SENDRESULTS = {122, "UDPUPLOADRESULT", "SENDRESULTS"};
__attribute__((unused))   static message_t M_UDPRESULT = {123, "SIMET_COMMONS_RESULT_UDP",NULL};
__attribute__((unused))   static message_t M_UDPUPLOADRESULT_DONE = {124, "UDPUPLOADRESULT","DONE"};


/*Jitter*/
__attribute__((unused))   static message_t M_JITTERUPLOADTIME = {151, "JITTERUPLOADTIME", "6"};
__attribute__((unused))   static message_t M_JITTERUPLOADTIME_OK = {152, "JITTERUPLOADTIME", "OK"};
__attribute__((unused))   static message_t M_JITTERUPLOADINTERVAL = {153, "JITTERUPLOADINTERVAL", "500"};
__attribute__((unused))   static message_t M_JITTERUPLOADINTERVAL_OK = {154, "JITTERUPLOADINTERVAL", "OK"};
__attribute__((unused))   static message_t M_JITTERUPLOAD_START = {156, "JITTERUPLOAD", "START"};
__attribute__((unused))   static message_t M_JITTERUPLOAD_OK = {157, "JITTERUPLOAD", "OK"};

__attribute__((unused))   static message_t M_JITTERDOWNLOADTIME = {161, "JITTERDOWNLOADTIME", "6"};
__attribute__((unused))   static message_t M_JITTERDOWNLOADTIME_OK = {162, "JITTERDOWNLOADTIME", "OK"};
__attribute__((unused))   static message_t M_JITTERDOWNLOADINTERVAL = {163, "JITTERDOWNLOADINTERVAL", "500"};
__attribute__((unused))   static message_t M_JITTERDOWNLOADINTERVAL_OK = {164, "JITTERDOWNLOADINTERVAL", "OK"};
__attribute__((unused))   static message_t M_JITTERDOWNLOAD_START = {165, "JITTERDOWNLOAD", "START"};
__attribute__((unused))   static message_t M_JITTERDOWNLOAD_OK = {166, "JITTERDOWNLOAD", "OK"};


__attribute__((unused))   static message_t M_JITTERPORT_OK = {155, "JITTERPORT", "OK"};
__attribute__((unused))   static message_t M_JITTERRESULT = {158, "SIMET_COMMONS_RESULT_JITTER", NULL};
__attribute__((unused))   static message_t M_JITTERRESULT_OK = {158, "SIMET_COMMONS_RESULT_JITTER", "OK"};
__attribute__((unused))   static message_t M_JITTERUPLOADRESULT_SENDRESULTS = {170, "JITTERUPLOADRESULT", "SENDRESULTS"};
__attribute__((unused))   static message_t M_JITTERUPLOADRESULT_DONE = {171, "JITTERUPLOADRESULT", "DONE"};
__attribute__((unused))   static message_t M_JITTERDOWNLOADRESULT_DONE = {172, "JITTERDOWNLOADRESULT", "DONE"};
__attribute__((unused))   static message_t M_JITTERDOWNLOADRESULT_OK = {172, "JITTERDOWNLOADRESULT", "OK"};
__attribute__((unused))   static message_t M_JITTERUPLOADDYNAMIC = {173, "JITTERUPLOADDYNAMIC", NULL };


/* Latency */
__attribute__((unused))   static message_t M_LATENCYUPLOAD_START = {333, "LATENCYUPLOAD", "START" };
__attribute__((unused))   static message_t M_LATENCYUPLOAD_OK = {334, "LATENCYUPLOAD", "OK" };
__attribute__((unused))   static message_t M_LATENCYDOWNLOAD_START = {335, "LATENCYDOWNLOAD", "START" };

#endif                          /* MESSAGES_H_ */

