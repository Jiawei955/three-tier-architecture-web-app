/*
 * http-server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv)
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) 
	die("signal() failed");
    if (argc != 5) {
        fprintf(stderr, "usage: %s <server-port> <web_root> <mdb-lookup-host> <mdb-lookup-port>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[1]);
    const char *webroot = argv[2];
    //const char *mdbip = argv[3];
    unsigned short mdbport = atoi(argv[4]);
    
    struct hostent *he;
    char *serverName = argv[3];
 
      // get server ip from server name
    if ((he = gethostbyname(serverName)) == NULL) {
         die("gethostbyname failed");
    }
    char *mdbip = inet_ntoa(*(struct in_addr *)he->h_addr);
    

    // Create a listening socket (also called server socket) 

    int servsock;
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    // Construct local address structure

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    servaddr.sin_port = htons(port);

    // Bind to the local address

    if (bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("bind failed");

    // Start listening for incoming connections

    if (listen(servsock, 5 /* queue size for connection requests */ ) < 0)
        die("listen failed");

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    FILE *fp;
    struct stat st;




// client socket to the mdb-lookup

    int sock; // socket descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    // Construct a server address structure

    struct sockaddr_in mdbservaddr;
    memset(&mdbservaddr, 0, sizeof(mdbservaddr)); // must zero out the structure
    mdbservaddr.sin_family      = AF_INET;
    mdbservaddr.sin_addr.s_addr = inet_addr(mdbip);
    mdbservaddr.sin_port        = htons(mdbport); // must be in network byte order

    // Establish a TCP connection to the server

    if (connect(sock, (struct sockaddr *) &mdbservaddr, sizeof(mdbservaddr)) < 0)
        die("connect failed");

    FILE *mdbinput = fdopen(sock,"r");

    // prepare respond
    char *correct_method = "GET";
    char error501[100];
    sprintf(error501,"HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>");
    char error400[100];
    sprintf(error400,"HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>");
    char error404[100];
    sprintf(error404,"HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>");
    char error301[800];
    char OK[100];
    sprintf(OK,"HTTP/1.0 200 OK\r\n\r\n");


    while (1) {

        // Accept an incoming connection

        clntlen = sizeof(clntaddr); // initialize the in-out parameter

        if ((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0)
        {
            perror("accept failed");
            continue;
            //die("accept failed");
        }

        // accept() returned a connected socket (also called client socket)
        // and filled in the client's address into clntaddr

        FILE *input = fdopen(clntsock,"r");
        char requestLine[4096];
        if (fgets(requestLine,sizeof(requestLine),input)==NULL){
            if (send(clntsock,error400,strlen(error400),0)!=strlen(error400))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 400 Bad Request\n",inet_ntoa(clntaddr.sin_addr), "  ");
            fclose(input);   
            continue;
        }
        // read all the headers
        char headers[5000];
        char *head;
        while((head=fgets(headers,sizeof(headers),input))){
            if (head==NULL)
                break;
            if (strcmp(head,"\r\n")==0)
                break;
        }
            
        // close connection if browser crashes in the middle of sending a request
        if (ferror(input)){
            fclose(input);
            continue;
        }


       
       
        // parsing the request line
        char requestLine2[100];
        strcpy(requestLine2,requestLine);
        if (requestLine2[strlen(requestLine2)-2]=='\r')
            requestLine2[strlen(requestLine2)-2]='\0';
        char *token_separators = "\t \r\n"; // tab, space, new line
	char *method = strtok(requestLine, token_separators);
	char *requestURI = strtok(NULL, token_separators);
	char *httpVersion = strtok(NULL, token_separators);
        char *leftover = strtok(NULL, token_separators);
        if (leftover){
            if (send(clntsock,error400,strlen(error400),0)!=strlen(error400))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 400 Bad Request\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            fclose(input);   
            continue;
        }
        

        if (!(method && requestURI && httpVersion)){
            if (send(clntsock,error400,strlen(error400),0)!=strlen(error400))
                
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 400 Bad Request\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            fclose(input);   
            continue;
        }
        
        //printf("%s\n",requestURI);
        
        int len_URI = strlen(requestURI);
        const char *form1 =
        "<html>\n"
        "<body>\n"
        "<h1>mdb-lookup</h1>\n"
        "<p>\n"
        "<form method=GET action=/mdb-lookup>\n"
        "lookup: <input type=text name=key>\n"
        "<input type=submit>\n"
        "</form>\n"
        "<p>\n"
        "</body>\n"
        "</html>\n";

        const char *form2 = 
        "<html>\n"
        "<body>\n"
        "<h1>mdb-lookup</h1>\n"
        "<p>\n"
        "<form method=GET action=/mdb-lookup>\n"
        "lookup: <input type=text name=key>\n"
        "<input type=submit>\n"
        "</form>\n"
        "<p>\n"
        "<p><table border>\n";

        const char *form2end =
        "</table>\n"
        "</body>\n"
        "</html>\n"; 


        // method not GET       
        if (strcmp(method,correct_method)!=0){
            if (send(clntsock,error501,strlen(error501),0)!=strlen(error501))
                
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 501 Not Implemented\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            fclose(input);   
            continue;
        }
        
        // wrong protocol/version
        char* correct_protocol1 = "HTTP/1.0";
        char* correct_protocol2 = "HTTP/1.1"; 
        if ((strcmp(httpVersion,correct_protocol1)!=0)&&(strcmp(httpVersion,correct_protocol2)!=0)){
            if (send(clntsock,error501,strlen(error501),0)!=strlen(error501))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 501 Not Implemented\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            fclose(input);
            continue;  
        }
        
        // request URI not start with "/"
        if (requestURI[0]!='/'){
            if (send(clntsock,error400,strlen(error400),0)!=strlen(error400))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 400 Bad Request\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            fclose(input);   
            continue;
        }
        
        // request URI does not contain /../ and does not end with /..
        char* x = "/../";
        char y[4]; 
        y[0] = requestURI[len_URI-3];
        y[1] = requestURI[len_URI-2];
        y[2] = requestURI[len_URI-1];
        y[3] = '\0';
        if ((strstr(requestURI,x))||(strcmp(y,"/.."))==0){
            if (send(clntsock,error400,strlen(error400),0)!=strlen(error400))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 400 Bad Request\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            fclose(input);   
            continue;
        }

        // handle the mdblookup
        if (strcmp(requestURI,"/mdb-lookup")==0){
            if(send(clntsock,OK,strlen(OK),0)!=strlen(OK))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 200 OK\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            if(send(clntsock,form1,strlen(form1),0)!=strlen(form1))
            {
                fclose(input);
                continue;
            }
            fclose(input);
            continue;
        }

        // handle the /mdb-lookup?key=xxx
        if (strlen(requestURI)>=16){
        char subbuff[17];
        strncpy(subbuff,requestURI,16);
        subbuff[16] = '\0';
        if (strcmp(subbuff,"/mdb-lookup?key=")==0){
            if(send(clntsock,OK,strlen(OK),0)!=strlen(OK))
            {
                fclose(input);
                continue;
            }
            fprintf(stdout,"%s \"%s\" 200 OK\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            char query[1005];
            int i = 16;
            while (requestURI[i]){
                query[i-16]=requestURI[i];
                i++;
            }
            query[i-16]='\n';
            query[i-15]='\0';
           // printf("%s\n",query);
           // send the query to mdb-lookup server
           if (send(sock,query,strlen(query),0)!=strlen(query))
           {

               if(send(clntsock,form2,strlen(form2),0)!=strlen(form2))
            {
                fclose(input);
                continue;
            }
                perror("send failed");
                fclose(input);
                continue;
           }
           // send to the client and receive from mdb-server
           char mdbrespond[100];
           char httprespond[150];
           if(send(clntsock,form2,strlen(form2),0)!=strlen(form2))
            {
                fclose(input);
                continue;
            }
           int color = 0;
           char *res;
           while ((res=fgets(mdbrespond,sizeof(mdbrespond),mdbinput))){
               if (strcmp(res,"\n")==0)
                  break; 
               if (color%2==0){

                   sprintf(httprespond,"%s%s","<tr><td>",mdbrespond);
                   if (send(clntsock,httprespond,strlen(httprespond),0)!=strlen(httprespond))
                       
                    {
                        fclose(input);
                        continue;
                    }

               }
               else {

                   sprintf(httprespond,"%s%s","<tr><td bgcolor=yellow>",mdbrespond);
                   if (send(clntsock,httprespond,strlen(httprespond),0)!=strlen(httprespond))
                    {
                        fclose(input);
                        continue;
                    }

               }
               
               color++;
           }
           if(!(res))
           {
               perror("send failed");
               fclose(input);
               continue;
           }
           if(send(clntsock,form2end,strlen(form2end),0)!=strlen(form2end))
            {
                fclose(input);
                continue;
            }
           fclose(input);
           continue;
        }
        }


        // append index.html
        char *index = "index.html";
        char searchingfile[len_URI+strlen(index)+5];
        if (requestURI[len_URI-1] == '/'){
            strcpy(searchingfile,requestURI) ;
            strcat(searchingfile,index);
        }
        else{
            strcpy(searchingfile,requestURI);
        }
        
        char filename[100];
        sprintf(filename,"%s%s",webroot,searchingfile);

        // directory not file
        if (stat(filename,&st)==0 && S_ISDIR(st.st_mode) && filename[strlen(filename)-1]!='/'){
 //           char location[100];
 //           sprintf(location,"http://clac.cs.columbia.edu%s/",requestURI);
            sprintf(error301,"HTTP/1.0 301 Moved Permanently\r\nLocation: http://clac.cs.columbia.edu:%s%s/\r\n\r\n",argv[1],requestURI); 
            fprintf(stdout,"%s \"%s\" 301 Moved Permanently\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            if(send(clntsock,error301,strlen(error301),0)!=strlen(error301))
            {
                fclose(input);
                continue;
            }
            fclose(input);
            continue;
        }

        //printf("%s\n",filename);

        if ((fp = fopen(filename,"rb")) == NULL){    // file not found 
            fprintf(stdout,"%s \"%s\" 404 Not Found\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            if (send(clntsock,error404,strlen(error404),0)!=strlen(error404))
            {
                fclose(input);
                continue;
            }
            fclose(input);
            continue;
        }
        else{ // log ok
            fprintf(stdout,"%s \"%s\" 200 OK\n",inet_ntoa(clntaddr.sin_addr), requestLine2);
            // read file to the buffer and send
            if(send(clntsock,OK,strlen(OK),0)!=strlen(OK))
            {
                fclose(input);
                continue;
            }
            char buf[4096];
            unsigned int n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                if (send(clntsock, buf, n, 0) != n)
                {
                    fclose(input);
                    continue;
                    //die("send content failed");
                }
            }
            if (ferror(fp)) {
                // fread() returns 0 on EOF or error, so we check if error occurred
                fclose(input);
                die("fread failed");
            }
            fclose(fp);
        }

        // Finally, close the client connection and go back to accept()
        fclose(input);
    }
}
