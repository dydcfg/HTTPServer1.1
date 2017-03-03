/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "parse.h"
#include "liso.h"
#include "log.h"
#include <fcntl.h>
#define ECHO_PORT 9990
#define BUF_SIZE 4096
#define MAXN 512
#define MAX_CONN 1024
#define TYPE_LEN 64
#define WEB_PATH "./static_site/"
#define LOG_PATH "./log/log.txt"

char buf[MAX_CONN+9][BUF_SIZE];
int bufLen[MAX_CONN+9], client_sock[MAX_CONN+9], client_close[MAX_CONN+9], nSockLeft = MAX_CONN;
struct Para params;

int close_socket(int sock)
{
    //printf("closing\n");
    if (close(sock))
    {
        Log("Failed closing socket.\n");
        return 1;
    }
    nSockLeft ++;
    return 0;
}

int closeClient(int id){
    int fd = client_sock[id];
    client_sock[id] = 0;
    client_close[id] = 1;
    bufLen[id] = 0;
    return close_socket(fd);    
}


int parseURI(Request *request, char *filename){
    strcpy(filename, params.wwwPath);
    if (!strstr(request->http_uri, "cgi-bin")){
        //static res
        strcat(filename, request->http_uri);
        if (request->http_uri[strlen(request->http_uri)-1] == '/')
            strcat(filename, "index.html");
        return 0;
    }
    else{
        //dynamic res
        return 1;
    }
    return 0;
}


int validateFile(int id, Request *request)
{
    struct stat sbuf;
    char filename[BUF_SIZE];

    parseURI(request, filename);
    // check if file exist
    if (stat(filename, &sbuf) < 0)
    {
        serveError(id, "404", "Not Found",
                    "Couldn't find this file.");
        return 0;
    }

    // check we have permission
    if ((!S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        serveError(id, "403", "Forbidden",
                    "Cannot read this file.");
        return 0;
    }

    return 1;
}

void parseFileType(char *filename, char *retBuff){
    
    if (strstr(filename, ".html"))
        strcpy(retBuff, "text/html");
    else if (strstr(filename, ".css"))
        strcpy(retBuff, "text/css");
    else if (strstr(filename, ".js"))
        strcpy(retBuff, "application/javascript");
    else if (strstr(filename, ".gif"))
        strcpy(retBuff, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(retBuff, "image/png");
    else if (strstr(filename, ".jpg") || strstr(filename, "jpeg"))
        strcpy(retBuff, "image/jpeg");
    else if (strstr(filename, ".wav"))
        strcpy(retBuff, "audio/x-wav");
    else
        strcpy(retBuff, "text/plain");
}


int servePost(int id, Request *request){
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char   content[BUF_SIZE], tbuf[TYPE_LEN], dbuf[TYPE_LEN];

    tm = *gmtime(&sbuf.st_mtime);
    strftime(tbuf, TYPE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, TYPE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    sprintf(content, "HTTP/1.1 204 No Content\r\n");
    if (client_close[id])
        sprintf(content, "%sConnection: close\r\n", content);
    else
        sprintf(content, "%sConnection: keep-alive\r\n", content);
    sprintf(content, "%sContent-Length: 0\r\n", content);
    sprintf(content, "%sContent-Type: text/html\r\n", content);    
    sprintf(content, "%sDate: %s\r\n", content, dbuf);
    sprintf(content, "%sServer: Liso/1.0\r\n\r\n", content);
    send(client_sock[id], content, strlen(content), 0);
    //printf("res:\n");
    //printf("%s\n",content);

    return 0;
}

int serveHead(int id, Request *request){
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char   filename[BUF_SIZE], content[BUF_SIZE], filetype[TYPE_LEN], tbuf[TYPE_LEN], dbuf[TYPE_LEN]; 
 
    if (validateFile(id, request) == 0) return 1;
    
    parseURI(request, filename);
    stat(filename, &sbuf);
    parseFileType(filename, filetype);
     
    tm = *gmtime(&sbuf.st_mtime);
    strftime(tbuf, TYPE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, TYPE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    sprintf(content, "HTTP/1.1 200 OK\r\n");
    if (client_close[id])
        sprintf(content, "%sConnection: close\r\n", content);
    else
        sprintf(content, "%sConnection: keep-alive\r\n", content); 
    sprintf(content, "%sContent-Length: %ld\r\n", content, sbuf.st_size);
    sprintf(content, "%sContent-Type: %s\r\n", content, filetype);
    sprintf(content, "%sDate: %s\r\n", content, dbuf);
    sprintf(content, "%sLast-Modified: %s\r\n", content, tbuf);
    sprintf(content, "%sServer: Liso/1.0\r\n\r\n", content);
    send(client_sock[id], content, strlen(content), 0);
    //printf("res:\n");
    //printf("%s\n",content);

    return 0;
}

int serveBody(int id, Request *request)
{
    int fd, filesize;
    char *ptr;
    char filename[BUF_SIZE];
    struct stat sbuf;
     
    parseURI(request, filename);

    if ((fd = open(filename, O_RDONLY, 0)) < 0)
    {
        Log("Error: Cann't open file \n");
        return -1;
    }

    stat(filename, &sbuf);

    filesize = sbuf.st_size;
    ptr = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    int cnt = send(client_sock[id], ptr, filesize, 0); 
    munmap(ptr, filesize);
    //printf("sent %d bits\n", cnt);
    return 0;
}

void serveError(int id, char *errorNum, char *shortMsg, char *longMsg){

    struct tm tm;
    time_t now;
    char header[BUF_SIZE], body[BUF_SIZE], dbuf[TYPE_LEN];

    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, TYPE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // response header
    sprintf(header, "HTTP/1.1 %s %s\r\n", errorNum, shortMsg);
    sprintf(header, "%sDate: %s\r\n", header, dbuf);
    sprintf(header, "%sServer: Liso/1.0\r\n", header);
    if (client_close[id]) 
        sprintf(header, "%sConnection: close\r\n", header);
    sprintf(header, "%sContent-type: text/html\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, (int)strlen(body));
    
    // response body
    sprintf(body, "<html><title>Liso Error</title>");
    sprintf(body, "%s<body>\r\n", body);
    sprintf(body, "%sError %s -- %s\r\n", body, errorNum, shortMsg);
    sprintf(body, "%s<br><p>%s</p></body></html>\r\n", body, longMsg);

    send(client_sock[id], header, strlen(header), 0);
    send(client_sock[id], body, strlen(body), 0);
}

int serveGet(int id, Request *request){
    serveHead(id, request);
    serveBody(id, request);
    return 0;
}

int handleClient(int id){
    Request *request = parse(buf[id], bufLen[id], client_sock[id]);
    //printf("Http Method %s\n",request->http_method);
    //printf("Http Version %s\n",request->http_version);
    //printf("Http Uri %s\n",request->http_uri);
    for(int index = 0;index < request->header_count;index++){
        //printf("Request Header\n");
        //printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
    }
    //Check method
    if (strcasecmp(request->http_method, "GET") && strcasecmp(request->http_method, "HEAD") && strcasecmp(request->http_method, "POST")){
        client_close[id] = 1;
        serveError(id, "501", "Not implemented", "This method is not support in liso.");
        free(request->headers);
        free(request);
        printf("handleClient ends.\n");
    }

    //Check HTTP version
    if (strcasecmp(request->http_version, "HTTP/1.1"))
    {
        client_close[id] = 1;
        serveError(id, "505", "HTTP Version not supported", "Liso only support HTTP/1.1");
        free(request->headers);
        free(request);
        printf("handleClient ends.\n");
    }
    if (strcasecmp(request->http_method, "GET") == 0){
        serveGet(id, request); 
    }
    else if (strcasecmp(request->http_method, "HEAD") == 0){
        serveHead(id, request);
    }
    else if (strcasecmp(request->http_method, "POST") == 0){
        servePost(id, request);
    }
    free(request->headers);
    free(request);
    return 0;
}

void paraError(){

    fprintf(stdout,
            "Usage: ./liso <HTTP port> <log file> <www folder>\n"
            );
    exit(EXIT_FAILURE);
}


int main(int argc, char* argv[])
{
    int master_sock, new_sock, n;
    ssize_t readret;
    fd_set readfds;
    //struct timeval tv;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    
    nSockLeft = MAX_CONN;
    
    if (argc != 4) paraError();

    params.port = (int)strtol(argv[1], (char**)NULL, 10);
    strcpy(params.logPath, argv[2]);
    strcpy(params.wwwPath, argv[3]);

    params.log = logOpen(params.logPath);

    Log("Server started.\n");

    fprintf(stdout, "----- Echo Server -----\n"); 

    for (int i = 0; i < MAXN; i ++){
        client_sock[i] = 0;
        bufLen[i] = 0;
        client_close[i] = 1;
    }

    //fprintf(stdout, "----- Echo Server -----\n"); 
    /* all networked programs must create a socket */
    if ((master_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        Log("Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    nSockLeft --;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(params.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(master_sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(master_sock);
        Log("Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    //fprintf(stdout, "----- Echo Server -----\n"); 
    if (listen(master_sock, 5))
    {
        close_socket(master_sock);
        Log("Error listening on socket.\n");
        return EXIT_FAILURE;
    }
    //fprintf(stdout,"tes");
    //tv.tv_sec = 0;
    //tv.tv_usec = 0;
    /* finally, loop waiting for input and then write it back */
    while (1)
    {
       
       FD_ZERO(&readfds);
       FD_SET(master_sock, &readfds);
       n = master_sock;
       
       for (int i = 0; i < MAXN; i ++){
           if (client_sock[i] > 0)
               FD_SET(client_sock[i], &readfds);
           if (client_sock[i] > n)
               n = client_sock[i];
       }
       if (select(n + 1, &readfds, NULL, NULL, NULL) == -1){
           Log("Select error!\n");
           return EXIT_FAILURE;
       }
       
       if (FD_ISSET(master_sock, &readfds)){
           if ((new_sock = accept(master_sock, (struct sockaddr *) &cli_addr, &cli_size)) == -1)
           {
               close(master_sock);
               Log("Error accepting connection.\n");
               return EXIT_FAILURE;
           }
           //fprintf(stdout,"New connection.\n");
           int newConnIdx = -1;
           for (int i = 0; i < MAXN; i++){
               if (client_sock[i] == 0){
                   client_sock[i] = new_sock;
                   bufLen[i] = 0;
                   client_close[i] = 0;
                   memset(buf[i], 0, BUF_SIZE);
                   printf("%d\n",i);
                   newConnIdx = i;
                   break;
               }
           }
           if (nSockLeft == 0){
               client_close[newConnIdx] = 1;
               serveError(new_sock, "503", "Service Unavailable", "Exceed maximum connection number limit.");
               closeClient(newConnIdx);
           }    
       }
       for (int i = 0; i < MAXN; i++){
           int sd = client_sock[i];
           if (FD_ISSET(sd, &readfds)){
               //printf("receiving from %d\n", i);
               cli_size = sizeof(cli_addr);

               readret = 0;

               if  ((readret = recv(sd, buf[i], BUF_SIZE, 0)) >= 1)
               {
                   bufLen[i] = readret;
                   /*
                   if (send(sd, buf, readret, 0) != readret)
                   {
                       close_socket(sd);
                       close_socket(master_sock);
                       fprintf(stderr, "Error sending to client.\n");
                       return EXIT_FAILURE;
                   }
                   */
                   //printf("%d th read %d bytes\n",i,(int)readret);
                   //printf("%s\n",buf[i]);
                   handleClient(i);
                   //closeClient(i);
               } 
               //fprintf(stdout,"%d end reading\n", i);
               if (readret == -1)
               {
                   closeClient(i);
                   close_socket(master_sock);
                   Log("Error reading from client socket.\n");
                   return EXIT_FAILURE;
               }
               
               if (client_close[i] == 1){
                   if (closeClient(i))
                   {
                       close_socket(master_sock);
                       Log("Error closing client socket.\n");
                       return EXIT_FAILURE;
                   }
              }
            }
        }
    }
    close_socket(master_sock);
    return EXIT_SUCCESS;
}
