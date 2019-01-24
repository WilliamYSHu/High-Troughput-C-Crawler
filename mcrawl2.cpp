#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>
#include <fstream>
#include <algorithm>
#include <set>
#include <queue>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
using namespace std;

const string myError="CUSTOM402ERROR";
const string my404="CUSTOM404NOTFOUND";
const string my402 = "CUSTOM402R";

void error(int number){
    switch(number){
        case 0:
            fprintf(stdout,"Operation successfully completed\n");
            return;
        case 1:
            fprintf(stderr,"input error\n");
            exit(1);
        case 2:
            fprintf(stderr,"socket error\n");
            exit(2);
        case 3:
            fprintf(stderr,"network error\n");
            exit(3);
        case 4:
            fprintf(stderr,"invalid hostname\n");
            exit(4);
        case 5:
            fprintf(stderr,"server down\n");
            exit(5);
        case 6:
            fprintf(stderr,"Chunk read error\n");
            exit(6);
        default:
            fprintf(stderr,"Generic error\n");
            exit(7);
    }
}

string hostname="";
int portno=80;
int numThread=1;
string fileDir="";
struct sockaddr_in serv_addr;
struct hostent *server;
int workStatus = 0; // if there is work processing
vector<pthread_t> myThreads;

pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;

// work queue
queue<string> workToDo;
set<string> workDone;

string getWork(){

     while(1){
       pthread_mutex_lock( &lock1 );
       if(workToDo.size() != 0 || workStatus == 0){
         pthread_mutex_unlock( &lock1 );
         break;
       }
       pthread_mutex_unlock( &lock1 );
     }

    //while (workToDo.size() == 0 && workStatus != 0);
    pthread_mutex_lock( &lock1 );
    if(workToDo.size() == 0 && workStatus == 0){
        pthread_mutex_unlock( &lock1 );
        return "";
    }
    workStatus ++;
    string result = workToDo.front();
    workToDo.pop();
    pthread_mutex_unlock( &lock1 );
    return result;
}

void decreaseWorkStat(){
    pthread_mutex_lock( &lock1 );
    workStatus --;
    pthread_mutex_unlock( &lock1 );
}

void restoreTask(string task){
    pthread_mutex_lock( &lock1 );
    workToDo.push(task);
    pthread_mutex_unlock( &lock1 );
}

void putWork(string work){
    pthread_mutex_lock( &lock1 );
    if (workDone.find(work) == workDone.end()) {
        workToDo.push(work);
        workDone.insert(work);
    }
    pthread_mutex_unlock( &lock1 );
}

string getCwd(string fullDir){
    reverse(fullDir.begin(), fullDir.end());
    ssize_t dashPos = fullDir.find("/");
    fullDir = fullDir.substr(dashPos);
    reverse(fullDir.begin(), fullDir.end());
    return fullDir;
}

string storedFileName(string fileName){
    for (int i = 0; i < fileName.size(); i++) {
        if (fileName[i] == '/') {
            fileName[i] = ':';
        }
    }
    string realName = fileDir + fileName;
    return realName;
}

string extractCookie(string HTMLRaw){
    ssize_t startPos = HTMLRaw.find("Set-Cookie: ") + 12;
    if (startPos == string::npos) {
        return "";
    }
    ssize_t endPos = HTMLRaw.find(";",startPos);
    string result = HTMLRaw.substr(startPos, endPos-startPos);
    return result;
}

ssize_t HTMLParser(string HTMLRaw, string cwd){
    ssize_t startPos;
    ssize_t endPos;
    ssize_t lastFind;
    string beginMark[4] = {"href=\"","HREF=\"","src=\"","href='"};
    ssize_t finalFind = 0;

    for (int i = 0; i < 4; i++) {
        string tempMark = beginMark[i];
        ssize_t sizeMark = tempMark.size();
        lastFind = 0;
        while ((startPos = HTMLRaw.find(tempMark, lastFind)) != string::npos) {
            startPos += sizeMark;
            lastFind = startPos;
            endPos = HTMLRaw.find("\"",lastFind);
            if (i == 3) endPos = HTMLRaw.find("'",lastFind);
            lastFind = endPos;
            string newLink = HTMLRaw.substr(startPos,endPos-startPos);

            // process the new link got
            if (newLink.size() == 0) continue;
            if (newLink[0] == '#') continue;
            if (newLink.size() >= 7 && newLink.substr(0,7) == "http://" && newLink.size() < hostname.size())
                continue;
            if (newLink.size() >= hostname.size() && newLink.substr(0,4) == "http"){
                newLink = newLink.substr(7);
                if (newLink.substr(0,hostname.size()) != hostname) {
                    continue;
                } else {
                    newLink = newLink.substr(hostname.size());
                    if (newLink.size() == 0) continue;
                }
            }

            if(newLink.find("?") != string::npos){
              newLink = newLink.substr(0,newLink.find("?"));
            }

            if (newLink.size() >= 4 && newLink.size() < hostname.size() && newLink.substr(0,4) == "http") continue;
            if((newLink.size() > 2  && newLink.substr(0,3) == "ht.") || (newLink.size() > 7  && newLink.substr(0,8) == "https://")) continue;

            if (newLink.substr(0,2) == "./" ) {
                newLink = cwd + newLink.substr(2);
            }
            if(newLink.substr(0,3) == "../"){
                if(cwd[cwd.size()-1] == '/') cwd = cwd.substr(0,cwd.size()-1);
                newLink = getCwd(cwd) + newLink.substr(3);
            }

            if (newLink[0] != '/') {
                newLink = "/" + newLink;
            }
            if(newLink.size() > 3 && newLink.substr(newLink.size()-4) == ".zip"){
              newLink = cwd.substr(0,cwd.size()-1) + newLink;
            }
            if (newLink[newLink.size()-1] == '/') {
                newLink = newLink + "index.html";
            }

            int checkalnum = 0;

            for(int i = 0; i < newLink.size(); i++){
              if((int)(newLink[i]) < 0 || (int)(newLink[i]) > 127){
                checkalnum = 1;
                break;
              }
            }

            if(checkalnum == 1) continue;

            putWork(newLink);
        }
        if (lastFind > finalFind) finalFind = lastFind;
    }
    return finalFind;
}

string readResp(int sockfd, int htmlFlag, string linkname, int cookieFlag, string & cookie, string cwd){
    const int buffLen = 2048;
    string filename = storedFileName(linkname);
    int my404Flag = 0;
    int my402Flag = 0;
    char buffer[buffLen];
    string HTMLfile = "";
    ssize_t n = 0; // record result of recv
    string readRaw = "";
    char* p; // current readed pointer
    ssize_t strPos = 0; // current readed bit
    ssize_t readedBit = 0;
    ssize_t bufferSize = 0; // record how many bits in buffer
    ssize_t recordedBit = 0; // record how many bits in file
    int chunkSize = 0;
    const int headLength = 512; // maximum Length of header

    // read header
    do {
        bzero(buffer, buffLen);
        if((n = recv(sockfd, buffer, headLength, 0)) < 0) error(3);
        if (n == 0) return myError;
        readedBit += n;
        bufferSize = n;
        readRaw += string(buffer);
    } while ((strPos = readRaw.find("\r\n\r\n")) == string::npos);

    // if need to read cookie
    if (cookieFlag == 0) {
        cookie = extractCookie(readRaw);
    }

    strPos += 4;
    ssize_t headerEnd = strPos;

    if(readRaw.substr(0,12) == "HTTP/1.1 500") error(5);
    if(readRaw.substr(0,12) == "HTTP/1.1 404") {
        my404Flag = 1;
        printf("%s: 404 not Found\n", linkname.c_str());
    }
    if(readRaw.substr(0,12) == "HTTP/1.1 402") {
        my402Flag = 1;
    }


    // get the size. Be causion if not in recieved bit
    ssize_t smallread = n;
    while (strstr(buffer+strPos, "\r\n") == NULL) {
        ssize_t m;
        if((m = recv(sockfd, buffer + smallread, buffLen-smallread, 0)) < 0) error(3);
        if (m == 0) return myError;
        smallread += m;
        bufferSize += m;
        //readRaw += string(buffer + smallread - m);
    }

    if (my402Flag == 1) {
      printf("%s: 402 Rate Limit\n",linkname.c_str());
      return my402;
    }


    // get size of chunk
    if(sscanf(buffer+headerEnd, "%x", &chunkSize) < 1) return myError;

    // get the start of chunk position in buffer
    p = strstr(buffer+headerEnd,"\r\n") + 2;
    readedBit = buffer + bufferSize - p;
    bufferSize = readedBit;
    memmove(buffer, p, readedBit);
    bzero(buffer+readedBit, buffLen-readedBit);

    FILE* tempFile;
    if(my404Flag == 0) tempFile = fopen(filename.c_str(),"wb");
    if(tempFile == NULL && my404Flag == 0) error(7);

    if(chunkSize == 0) return myError;

    while (chunkSize != 0) {
        recordedBit = 0;
        while (readedBit < chunkSize){
            if (my404Flag == 0 && htmlFlag == 1){
                char buff[bufferSize+3];
                strncpy(buff, buffer, bufferSize);
                buff[bufferSize] = '\0';
                HTMLfile += string(buff);
                ssize_t lastPos = HTMLParser(HTMLfile, cwd);
                HTMLfile = HTMLfile.substr(lastPos);
            }
            if(my404Flag == 0) fwrite(buffer, bufferSize, 1, tempFile);
            recordedBit += bufferSize;
            bzero(buffer, buffLen);
            if ((bufferSize = recv(sockfd, buffer, buffLen, 0)) < 0) error(3);
            if(bufferSize == 0){
                fclose(tempFile);
                return myError;
            }

            readedBit += bufferSize;
        }
        if(my404Flag == 0) fwrite(buffer, chunkSize-recordedBit, 1, tempFile);
        if (my404Flag == 0 && htmlFlag == 1){
            char buff[chunkSize-recordedBit+3];
            strncpy(buff, buffer, chunkSize-recordedBit);
            buff[chunkSize-recordedBit] = '\0';
            HTMLfile += string(buff);
            ssize_t lastPos = HTMLParser(HTMLfile, cwd);
            HTMLfile = HTMLfile.substr(lastPos);
        }

        // finish reading one chunk, find another
        memmove(buffer, buffer + chunkSize-recordedBit, bufferSize - chunkSize + recordedBit);
        bzero(buffer+bufferSize - chunkSize + recordedBit, buffLen-(bufferSize - chunkSize + recordedBit));
        //readRaw = string(buffer);
        smallread = bufferSize - chunkSize + recordedBit;
        bufferSize = smallread;

        // find first /r/n followed by chunk
        while (strstr(buffer,"\r\n") == NULL) {
            ssize_t m;
            if((m = recv(sockfd, buffer + smallread, buffLen-smallread, 0)) < 0) error(3);
            if(m == 0){
                fclose(tempFile);
                return myError;
            }
            smallread += m;
            bufferSize += m;
            //readRaw += string(buffer + smallread - m);
        }
        strPos = (strstr(buffer,"\r\n") - buffer)+2;

        // find next chunk size and /r/n
        while (strstr(buffer + strPos, "\r\n") == NULL) {
            ssize_t m;
            if((m = recv(sockfd, buffer + smallread, buffLen-smallread, 0)) < 0) error(3);
            if(bufferSize == 0){
                if(my404Flag == 0) fclose(tempFile);
                return myError;
            }
            smallread += m;
            bufferSize += m;
            //readRaw += string(buffer + smallread - m);
        }

        if(sscanf(buffer+strPos, "%x", &chunkSize) < 1) return myError;

        p = strstr(buffer + strPos, "\r\n") + 2;
        readedBit = buffer + bufferSize - p;
        bufferSize = readedBit;
        memmove(buffer, p, readedBit);
        bzero(buffer+readedBit, buffLen-readedBit);
    }
    if(my404Flag == 0) fclose(tempFile);

    return HTMLfile;
}



string createHTTPGet(string path) {
    string request = "";
    request += "GET " + path + " HTTP/1.1\r\n";
    request += "HOST: " + hostname + "\r\n\r\n";
    //request += "Connection: close\r\n\r\n";
    return request;
}

string createHTTPGetWCookie(string path, string cookie) {
    string request = "";
    request += "GET " + path + " HTTP/1.1\r\n";
    request += "HOST: " + hostname + "\r\n";
    request += "Cookie: " + cookie + "\r\n\r\n";
    //request += "Connection: close\r\n\r\n";
    return request;
}

void writeMessage(int sockfd, string msg){
    ssize_t m = write(sockfd,msg.c_str(),msg.size());
    if (m < 0) {
        printf("send failed\n");
    }
}

void* singleCrawler(void* args){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error(2);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error(3);
    string wLink="";
    int cookieFlag = 0; // mark if there is cookie;
    string cookie="";

    while ((wLink = getWork()) != "") {
        string cwd = getCwd(wLink);

        printf("Crawling link: %s\n", wLink.c_str());
        // mode when the task is html
        if (wLink.size() >= 5 && wLink.substr(wLink.size()-5) == ".html") {

            if (cookieFlag == 0) {
                string request = createHTTPGet(wLink);
                writeMessage(sockfd, request);
                string response = readResp(sockfd, 1, wLink, 0, cookie,cwd);
                if(response == myError){
                    close(sockfd);
                    sockfd = socket(AF_INET, SOCK_STREAM, 0);
                    if (sockfd < 0) error(2);
                    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error(3);
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();
                    continue;
                }
                if(response == my402){
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();
                    continue;
                }
                cookieFlag = 1;
            } else {
                string request = createHTTPGetWCookie(wLink, cookie);
                writeMessage(sockfd, request);
                string response = readResp(sockfd, 1, wLink, 1, cookie,cwd);
                if(response == myError){
                    close(sockfd);
                    sockfd = socket(AF_INET, SOCK_STREAM, 0);
                    if (sockfd < 0) error(2);
                    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error(3);
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();
                    continue;
                }
                if(response == my402){
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();
                    continue;
                }
            }
        }
        else{
            if (cookieFlag == 0) {
                string request = createHTTPGet(wLink);
                writeMessage(sockfd, request);
                string response = readResp(sockfd, 0, wLink, 0, cookie,cwd);
                if(response == myError){
                    close(sockfd);
                    sockfd = socket(AF_INET, SOCK_STREAM, 0);
                    if (sockfd < 0) error(2);
                    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error(3);
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();

                    continue;
                }
                if(response == my402){
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();
                    continue;
                }
                cookieFlag = 1;
            } else {
                string request = createHTTPGetWCookie(wLink, cookie);
                writeMessage(sockfd, request);
                string response = readResp(sockfd, 0, wLink, 1, cookie,cwd);
                if(response == myError){
                    close(sockfd);
                    sockfd = socket(AF_INET, SOCK_STREAM, 0);
                    if (sockfd < 0) error(2);
                    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error(3);
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();

                    continue;
                }
                if(response == my402){
                    cookieFlag = 0;
                    restoreTask(wLink);
                    decreaseWorkStat();
                    continue;
                }
            }
        }
        decreaseWorkStat();
    }
    close(sockfd);
    return NULL;
}

int main(int argc, char* argv[]){
    string readTemp;

    // start parse
    if (argc < 5) error(1);
    if ((argc-5) %2 == 1) error(1);
    for (int i = 1; i < argc; i += 2) {
        if (!(strcmp(argv[i], "-p"))) {
            if (sscanf(argv[i+1],"%d" ,&portno) == 0) error(1);
        } else if(!(strcmp(argv[i], "-n"))){
            if (sscanf(argv[i+1],"%d" ,&numThread) == 0) error(1);
        } else if(!(strcmp(argv[i], "-h"))){
            hostname = string(argv[i+1]);
        } else if (!(strcmp(argv[i], "-f"))){
             fileDir = string(argv[i+1]);
        } else error(1);
    }

    // end parse


    if(hostname.size() >= 7 && hostname.substr(0,7) == "http://") hostname = hostname.substr(7);
    size_t hndash = hostname.find("/");
    if (hndash != string::npos) {
        hostname = hostname.substr(0,hndash);
    }

    if(numThread <1) error(1);

    // process the file name
    if (fileDir[fileDir.size()-1] != '/') fileDir = fileDir + "/";

    if (mkdir(fileDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
    {
        if( errno != EEXIST ) {
            fprintf(stderr,"can't create directory\n");
            exit(1);
        }
    }


    // get server address and portno
    server = gethostbyname(hostname.c_str());

    if (server == NULL) error(4);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);

    workToDo.push("/index.html");
    workDone.insert("/index.html");
    for (int i = 0; i < numThread; i++) {
        pthread_t tempID;
        if (pthread_create(&tempID, NULL, singleCrawler, NULL) != 0) {
          fprintf(stderr, "Error creating threads\n");
          error(7);
        }
        pthread_mutex_lock(&lock1);
        myThreads.push_back(tempID);
        pthread_mutex_unlock(&lock1);
    }
    for (int i = 0; i < numThread; i++) {
        pthread_join(myThreads[i], NULL);
    }

    error(0);
    return 0;

}
