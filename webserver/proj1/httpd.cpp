#include <iostream>
#include "httpd.h"
#include "httpFramer.hpp"
#include <stdio.h>  /* for perror() */
#include <stdlib.h> /* for exit() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <map>
#include <unistd.h>     /* for close() */
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <time.h>


using namespace std;
const int MAXPENDING = 5;
const int BUFSIZE = 1024;
char* docRoot;
const string SPACE = " ";
const string CRLF = "\r\n";
const string SERVER = "Super.M";

void HandleTCPClient(int clntSocket);   /* TCP client handling function */

// ------------------------Die with error function-----------------------
void DieWithUserMessage(const char *msg, const char *detail) {
    fputs(msg, stderr);
    fputs(": ", stderr);
    fputs(detail, stderr);
    fputc('\n', stderr);
    exit(1);
}

void DieWithSystemMessage(const char *msg) {
    perror(msg);
    exit(1);
}



// Structure of arguments to pass to client thread
struct ThreadArgs {
    int clntSock; // Socket descriptor for client
};


//------------------ HTTPRequest Structure-----------------

struct HttpRequest {            // Parsed client message
    // initial line
    string method;
    string url;
    string version;
    string realpath;
    
    // key: value
    string host;
    string connection;
    bool isMalformed;
};

//------------------ HTTPResponse Structure-----------------

struct HttpResponse {            // Response components
    // initial line
    string version;
    int statusCode;
    string codeMsg;
    
    // key: value
    string lastMod;
    string conType;
    string contLen;
    bool close;
};

//--------------------------Thread handler-------------------------
void *ThreadMain(void *threadArgs) {
    // Guarantees that thread resources are deallocated upon return
    pthread_detach(pthread_self());
    
    // Extract socket file descriptor from argument
    int clntSock = ((struct ThreadArgs *) threadArgs)->clntSock;
    free(threadArgs); // Deallocate memory for argument
    
    HandleTCPClient(clntSock);
    
    return (NULL);
}

//----------------------helper function-------------------------
string strip(const string &s) {
    int i = 0, j = (int)s.size() - 1;
    while (i <= j && isspace(s[i])) ++i;
    while (i <= j && isspace(s[j])) --j;
    return s.substr(i, j - i + 1);
}

bool escapeRoot(const string &path, const string &doc_root) {
    return path.size() < doc_root.size() ||
    path.substr(0, doc_root.size()) != doc_root;
}


//------------------ HTTPRequest Parser function-----------------
HttpRequest parseRequest(string message) {
    HttpRequest req;
    size_t pos = 0, afterPos = 0;
    string init[3];
    // check format of 1st line
    pos = message.find_first_of(CRLF, afterPos);
    string initial = message.substr(afterPos, pos - afterPos);
    if (initial.size() != strip(initial).size()){
        req.isMalformed = true;
        return req;
    }
    

    // assign field in initial line
    pos = 0;
    afterPos = 0;
    for (int i = 0; i < 2; i++) {
        pos = initial.find_first_of(SPACE, afterPos);
        if (isspace(initial[pos+1])){
            req.isMalformed = true;
            return req;
        }
        init[i] = initial.substr(afterPos, pos - afterPos);
        afterPos = pos + 1;
    }
    pos = message.find_first_of(CRLF, afterPos);
    init[2] = message.substr(afterPos, pos - afterPos);
    afterPos = pos + 2; //start of 2nd line
    req.method = init[0];
    if (init[1] == "/") {
        req.url = "/index.html";
    } else {
        req.url = init[1];
    }
    req.version = init[2];
    
    
    // check format of each key:value line
    while (pos != -1 && afterPos < message.size()) {
        pos = message.find_first_of(CRLF, afterPos);
        string line = message.substr(afterPos, pos-afterPos);
        int colon = (int)line.find(":");
        if (colon == -1 || isspace(line[colon-1])){
            req.isMalformed = true;
            return req;
        } else {
            afterPos = pos + 2;
        }
    }

    if (message.find("Host:") != std::string::npos){
        pos = message.find("Host:");
        afterPos = message.find_first_of(CRLF, pos);
        req.host = message.substr(pos + 5, afterPos - pos-5);
    } else {
        req.isMalformed = true;
        return req;
    }
    
    if (message.find("Connection:") != std::string::npos){
        pos = message.find("Connection:");
        afterPos = message.find_first_of(CRLF, pos);
        req.connection = message.substr(pos + 11, afterPos - pos-11);
    }
    return req;
}

HttpResponse serveResponse(HttpRequest req, string doc_root){
    HttpResponse res;
    if(doc_root.back() == '/'){
        doc_root.pop_back();
    }
    string reqPath = doc_root.append(req.url);
    
    struct stat sb;
    char pathBuf[1024];
    realpath(reqPath.c_str(), pathBuf);
    string resPath(pathBuf);
    int flag = stat(resPath.c_str(), &sb);
    
    // 400 error : malformed or invalid request
    if (req.method != "GET" || req.url.substr(0,1) != "/" || req.version != "HTTP/1.1") {
        res.version = "HTTP/1.1";
        res.statusCode = 400;
        res.codeMsg = "Client Error";
        res.close = true;
        return res;
    } else if (req.host.length() == 0 || req.isMalformed){
            res.version = "HTTP/1.1";
            res.statusCode = 400;
            res.codeMsg = "Client Error";
            res.close = true;
            return res;
    }
    
    
    
    // File not exist
//    if (!p) {
//        res.version = "HTTP/1.1";
//        res.statusCode = 404;
//        res.codeMsg = "Not Found";
//        return res;
//    }
    
    //404 Not Found: The requested content wasn’t there
    else if (flag != 0) {
        res.version = "HTTP/1.1";
        res.statusCode = 404;
        res.codeMsg = "Not Found";
        res.close = true;
        return res;
    }
    
//    if (stat(resPath.c_str(), &sb) != 0) {
//        res.version = "HTTP/1.1";
//        res.statusCode = 404;
//        res.codeMsg = "Not Found";
//        return res;
//    }
    
    //403 Forbidden: The request was not served because the client wasn’t allowed to access the requested content
    else if (!(sb.st_mode & S_IROTH)) {
        res.version = "HTTP/1.1";
        res.statusCode = 403;
        res.codeMsg = "Forbidden";
        res.close = true;
        return res;
    }
    // 200
    else {
//        int in_fd = open(resPath.c_str(), O_RDONLY);
//        if (in_fd < 0)
//            DieWithSystemMessage("Open failed");
        // Prepare response
        res.version = "HTTP/1.1";
        res.statusCode = 200;
        res.codeMsg = "OK";
        
        stat(resPath.c_str(), &sb);
        
        // Getting last-modified
        char lastMod[1024];
        time_t t = sb.st_mtime;
        struct tm *gmt;
        gmt = gmtime(&t);
        strftime(lastMod, 1024, "%a, %d %b %Y %H:%M:%S %Z", gmt);
        string time(lastMod);
        res.lastMod = time;
//        size_t pos;
//        size_t dotdot = req.url.find("..");
//        if (dotdot != -1) {
//            pos = req.url.find_first_of(".", dotdot + 2);
//        } else {
//            pos = req.url.find(".");
//        }
        size_t pos = req.url.find_last_of(".");
        string ext = req.url.substr(pos+1, strlen(req.url.c_str()) - pos-1);
        string type;
        if (ext == "jpg")
            type = "image/jpeg";
        else if (ext == "html")
            type = "text/html";
        else if (ext == "png")
            type = "image/png";
        else
            type = "text/plain";
        res.conType = type;
        // Getting content length
        string size = to_string(sb.st_size);
        
        res.contLen = size;
    }
    return res;
}

//------------------ HTTPResponse function-----------------
void ReturnMsg(int clntSocket, HttpResponse res, string realpath) {
    string message;
    
    message.append(res.version + SPACE);
    message.append(to_string(res.statusCode) + SPACE);
    message.append(res.codeMsg + CRLF);
    message.append("Server: " + SERVER + CRLF);
    if (!res.close) {
        message.append("Last-Modified: " + res.lastMod + CRLF);
        message.append("Content-Type: " + res.conType + CRLF);
        message.append("Content-Length: " + res.contLen + CRLF);
    }
    
    message.append(CRLF);
    const char *header = message.c_str();
    ssize_t len = strlen(header);
    
    
    ssize_t numBytesSent = send(clntSocket, header, len, 0);
    if (numBytesSent < 0) {
        DieWithSystemMessage("send() failed");
    }
    
    ssize_t sentfile_cnt = 0;

    if (res.statusCode == 200) {
        off_t* offset = 0;
        size_t length;
        struct stat sb;
        if (stat(realpath.c_str(), &sb) == -1) {
            perror("stat");
            exit(EXIT_FAILURE);
        }
        length = sb.st_size;
        int in_fd = open(realpath.c_str(), O_RDONLY);
        if (in_fd < 0)
            DieWithSystemMessage("Open failed");
        while (sentfile_cnt < (ssize_t)length) {
            ssize_t numBytesSent = sendfile(clntSocket, in_fd, offset, length);
            if (numBytesSent < 0)
                DieWithSystemMessage("sendfile() failed");
            else if (numBytesSent == 0)
                DieWithUserMessage("send()", "failed to send anything");
            sentfile_cnt += numBytesSent;
        }
    }
}

// -------------------Handle TCP Client--------------------------
void HandleTCPClient(int clntSock) {
    char buffer[BUFSIZE]; // Buffer for echo string
    memset(buffer, 0, BUFSIZE); // zero out buffer
    Framer framer;
    ssize_t numBytesRcvd;
    struct HttpRequest reqMsg;
    
    
    while (true) {
        // Receive message from client
        numBytesRcvd = recv(clntSock, buffer, BUFSIZE-1,0);
        if (numBytesRcvd <= 0){
            // If timer reaches 5, time the client out and close socket
            if(numBytesRcvd < 0 && errno==EWOULDBLOCK){
                string msg = "HTTP/1.1 400 Client Error\r\n";
                const char *header = msg.c_str();
                ssize_t len = strlen(header);
                send(clntSock, header, len, 0);
                close(clntSock);
                return;
            }
            else{
                DieWithSystemMessage("recv() failed");
                close(clntSock);
                return;
            }
        }
        
        buffer[numBytesRcvd] = '\0';
        string s(buffer);
        framer.append(s);
        
        while (framer.hasMessage()){
            string message = framer.topMessage();
            framer.popMessage();
            
            HttpRequest req = parseRequest(message);
            
            string doc_root(docRoot);
            HttpResponse res = serveResponse(req, doc_root);
            
            //combine root_path with req_path
            if(doc_root.back() == '/'){
                doc_root.pop_back();
            }
            string reqPath = doc_root.append(req.url);

            char pathbuf[1024];
            realpath(reqPath.c_str(), pathbuf);
            string resPath(pathbuf);
            
            ReturnMsg(clntSock, res, resPath);
            if (req.connection.find("close") != -1 || req.isMalformed){
                close(clntSock);
                return;
            }
        }
    }
    close(clntSock);
}


void start_httpd(unsigned short port, string doc_root)
{
    
    cerr << "Starting server (port: " << port <<
    ", doc_root: " << doc_root << ")" << endl;
    int serverSock;                  /* Socket descriptor for server */
    int clntSock;                    /* Socket descriptor for client */
    struct sockaddr_in servAddr;                  // Local addresssocklen_t
    
    if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithSystemMessage("socket() failed");
    
    // Construct local address structure
    memset(&servAddr, 0, sizeof(servAddr));       // Zero out structure
    servAddr.sin_family = AF_INET;                // IPv4 address family
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
    servAddr.sin_port = htons(port);          // Local port
    
    // Bind to the local address
    if (::bind(serverSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
        DieWithSystemMessage("bind() failed");
    
    // Mark the socket so it will listen for incoming connections
    if (listen(serverSock, MAXPENDING) < 0)
        DieWithSystemMessage("listen() failed");
    
    docRoot = (char*) malloc(doc_root.size());
    strcpy(docRoot,doc_root.c_str());
    
    for (;;) { // Run forever
        struct sockaddr_in clntAddr;                   // Client address
        socklen_t clntLen = sizeof(clntAddr);
        
        /* Wait for a client to connect */
        if ((clntSock = accept(serverSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            DieWithSystemMessage("accept() failed");
        
        char clntName[INET_ADDRSTRLEN]; // String to contain client address
        if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName,sizeof(clntName)) != NULL){
            printf("Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
        } else {
            puts("Unable to get client address");
        }
        
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        if (setsockopt(clntSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
            DieWithUserMessage("setsockopt", "SO_RCVTIMEO");
        
        if (setsockopt(clntSock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
            DieWithUserMessage("setsockopt", "SO_SNDTIMEO");

        
        // Create separate memory for client argument
        struct ThreadArgs *threadArgs = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs));
        if (threadArgs == NULL)
            DieWithSystemMessage("malloc() failed");
        threadArgs->clntSock = clntSock;
        
        // Create client thread
        pthread_t threadID;
        int returnValue = pthread_create(&threadID, NULL, ThreadMain, threadArgs);
        if (returnValue != 0)
            DieWithUserMessage("pthread_create() failed", strerror(returnValue));
        printf("with thread %ld\n", (long int) threadID);
    }
}
