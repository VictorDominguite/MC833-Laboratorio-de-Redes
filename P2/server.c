#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <netdb.h>
#include "cJSON/cJSON.h"

#define MYPORT "3221"    // the port users will be connecting to
#define MAXBUFLEN 10000
#define MAXIDLEN 5
#define MAXYEARLEN 5
#define MAXSUBSECTIONLEN 50
#define MAXCHORUSLEN 200
#define BACKLOG 10   // how many pending connections queue will hold
#define HEADERBUFSIZELEN 5

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

cJSON* read_json_string(char* buffer){
    /*
        reads a string from a buffer and returns it as a cJSON json object
    */
    cJSON *json = cJSON_Parse(buffer); 
    if (json == NULL) { 
        const char *error_ptr = cJSON_GetErrorPtr(); 
        if (error_ptr != NULL) { 
            printf("Error: %s\n", error_ptr); 
        } 
        cJSON_Delete(json); 
        return NULL; 
    } 
    return json;
}

cJSON* read_json() {
    /*
        reads songs_data.json file and returns its content as a cJSON json object
    */
    FILE *fp = fopen("songs_data.json", "r"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n"); 
        return NULL; 
    } 

    char buffer[MAXBUFLEN]; 
    fread(buffer, 1, sizeof(buffer), fp); 
    fclose(fp); 

    return read_json_string(buffer);
}

void write_json(cJSON* json) { 
    /*
        receives a cJSON json object, turns it into a string and write it to songs_data.json file
    */
   char *json_str = cJSON_Print(json); 
  
   // write the JSON string to a file 
   FILE *fp = fopen("songs_data.json", "w"); 
   if (fp == NULL) { 
       printf("Error: Unable to open the file.\n"); 
       return; 
   } 
   fputs(json_str, fp); 
   fclose(fp); 
}
    
char* read_request(char *request,  cJSON *json){
    /*
        reads received request and returns a response buffer.
        the buffers content depends on what operation was requested.
    */
    printf("request: %s\n", request);
    cJSON *elem;
    cJSON *name;
    cJSON *name_fetched;
    cJSON *id_fetched;
    cJSON* songs = cJSON_CreateArray();
    char genre[MAXSUBSECTIONLEN];
    char year[MAXYEARLEN];
    char literal_name[MAXSUBSECTIONLEN];
    char* response = (char*)malloc(MAXBUFLEN);
    int n = cJSON_GetArraySize(json);
    int offset = HEADERBUFSIZELEN + 3;
    switch (request[HEADERBUFSIZELEN+1])
    {
    case '0':
        strcpy(response, "1");
        break;
    case '1':
        //1. Register a new song
        // returns 0/1 if failed/succeded
        cJSON* song = read_json_string(offset+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            id_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(id_fetched->valuestring, cJSON_GetObjectItem(song, "ID")->valuestring) == 0){
                strcpy(response, "0"); //cannot register a song with an existing ID
                return response;
            }
        }
        cJSON_AddItemToArray(json, song);
        write_json(json);
        strcpy(response, "1");
        break;
    case '2':
        //2. Delete a song
        // returns 0/1 if failed/succeded
        char id[MAXIDLEN];
        strcpy(id, offset+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            id_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(id_fetched->valuestring, id) == 0){
                cJSON_DeleteItemFromArray(json,i);
                write_json(json);
                strcpy(response, "1");
                return response;
            }
        }
        strcpy(response, "0"); //ID doesnt exist
        break;
    case '3':
        //3. List all songs from a year
        // returns [song_object, ...] being song_object each of the songs from the year requested 
        strcpy(year, offset+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name = cJSON_GetObjectItem(elem, "Release Date");
            if(strcmp(name->valuestring, year) == 0){

                cJSON_AddItemReferenceToArray(songs, elem);
            }
        }
        strcpy(response, cJSON_Print(songs));
        break;
    case '4':
        //4. List all songs of a certain language from a certain year
        // returns [song_object, ...] being song_object each of the songs from the year and language requested  
        char language[MAXSUBSECTIONLEN];
        cJSON *language_fetched;
        strcpy(language, offset+5+request);
        strncpy(year, offset+request, 4);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name = cJSON_GetObjectItem(elem, "Release Date");
            language_fetched = cJSON_GetObjectItem(elem, "Language");
            if(strcmp(name->valuestring, year) == 0 && strcmp(language_fetched->valuestring, language) == 0){
                cJSON_AddItemReferenceToArray(songs, elem);
            }
        }
        strcpy(response, cJSON_Print(songs));
        break;
    case '5':
        //5. List all songs of a genre
        // returns [song_object, ...] being song_object each of the songs from thegenre requested    
        cJSON *genre_fetched;
        strcpy(genre, offset+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            genre_fetched = cJSON_GetObjectItem(elem, "Genre");
            if(strcmp(genre_fetched->valuestring, genre) == 0){
                cJSON_AddItemReferenceToArray(songs, elem);
            }
        }
        strcpy(response, cJSON_Print(songs));
        break;
    case '6':
        //6. List all info from a certain song
        // returns [json object]/[] if the ID exists/does not exist
        strcpy(literal_name, offset+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(name_fetched->valuestring, literal_name) == 0){
                cJSON_AddItemReferenceToArray(songs, elem);
                break;
            }
        }
        strcpy(response, cJSON_Print(songs));
        break;
    case '7':
        //7. List all the information from all songs
        // returns [song_object, ...] being song_object each of the songs from database 
        strcpy(response, cJSON_Print(json));
        break;
    default:
        break;
    }
    return response;
}

void *get_in_addr(struct sockaddr *sa)
    /*
        get sockaddr
    */
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void server_listen(int sockfd){
    /*
    Announce willingness to accept connections, give queue size
    change socket state for TCP server.
    */
    int listen_response = listen(sockfd, BACKLOG);

    if (listen_response == -1) {
        perror("listen");
        exit(1);
    }
    printf("server: waiting for connections...\n");
}

int server_accept(int sockfd, struct sockaddr_storage their_addr){
    /*
        Return next completed connection
    */
    socklen_t sin_size;
    sin_size = sizeof their_addr;
    int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) perror("accept");
    return new_fd;
}

int create_socket(int type){
    /*
    Creates a new socket and binds it based on server's IP and port determined

    */
    int rv, sockfd;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE; 
    if (type == 0){
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
    }
    else{
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_DGRAM;
    }

    // get full adress from IP and port
    rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    //create socket
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) perror("server: socket");
    //bind socket with adress
    rv = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (rv == -1) {
        close(sockfd);
        perror("server: bind");
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

void padding(char* final_string, char* s, int size){
    int pad = size - strlen(s);
    strcpy(final_string, "\0");
    char* pad_item = "0";
    for(int i = 0; i<pad; i++){
        strcat(final_string, pad_item);
    }
    strcat(final_string, s);
}

void attach_buf_size_header(char* buf){
    char final_string[MAXBUFLEN];
    int size = strlen(buf) + HEADERBUFSIZELEN + 1;
    char size_str[HEADERBUFSIZELEN];
    sprintf(size_str, "%d", size);  
    padding(final_string, size_str, HEADERBUFSIZELEN);
    strcat(final_string, "/");
    strcat(final_string, buf);
    strcpy(buf, final_string);
}

void attach_number_series_header(char* final_string, char* buf, int packet_count) {
    char count_str[7];
    sprintf(count_str, "%d", packet_count);
    padding(final_string, count_str, HEADERBUFSIZELEN);
    strcat(final_string, "/");
}


int send_all(int sockfd, char* buf) {
    int total_sent = 0, n;
    int bytes_to_send = strlen(buf);
    int total_to_send = strlen(buf);

    while(total_sent < total_to_send){
        // Sends operation request to server
        if ((n = write(sockfd, buf, bytes_to_send)) == -1) {
            perror("send");
            return -1;
        }
        total_sent += n;
        bytes_to_send -= n;
    }
    return 0;
}

int read_all(int sockfd, char* response) {
    char partial_response[MAXBUFLEN];
    int total_received = 0, total_to_receive = -1;
    int numbytes_read;
    char str_total_to_receive[HEADERBUFSIZELEN + 1];

    response[0] = '\0';

    // Try reading the whole message, or at least the first bytes to know what is 
    // the size of the message
    do {
        if ((numbytes_read = read(sockfd, partial_response, MAXBUFLEN-1)) == -1) {
            perror("read");
            return -1;
        }
        total_received += numbytes_read;
        partial_response[numbytes_read] = '\0';
        strcat(response, partial_response);
        response[total_received] = '\0';

    } while(total_received < HEADERBUFSIZELEN);

// Get the total bytes that are going to be received
    strncpy(str_total_to_receive, response, HEADERBUFSIZELEN);
    str_total_to_receive[HEADERBUFSIZELEN] = '\0';
    total_to_receive = atoi(response);

    // Receives the rest of the message, if there are still any bytes left to read
    while(total_received < total_to_receive) {
        if ((numbytes_read = read(sockfd, partial_response, MAXBUFLEN-1)) == -1) {
            perror("read");
            return -1;
        }
        total_received += numbytes_read;
        partial_response[numbytes_read] = '\0';
        strcat(response, partial_response);
        response[total_received] = '\0';
    }

    return 0;
}

int service(int new_fd, cJSON* json){
    char buf[MAXBUFLEN];
    char* response;
    // respond properly to the request:
    read_all(new_fd, buf);
    response = read_request(buf, json);// act!
    
    attach_buf_size_header(response);
    send_all(new_fd, response);

    free(response);
    if (buf[HEADERBUFSIZELEN+1] == '0') return 0; // terminate connection
    return 1;
}

int max(int first, int second){
    int m = 0;
    if (first > m) m = first;
    if (second > m) m = second;
    return m;
}

void read_file(char* ID, char* song_buf){
    char name[100] = "songs/";
    strcat(name, ID);
    strcat(name, ".mp3");
    printf("%s\n", name);
    FILE *fp;
    fp=fopen(name,"rb");
    FILE *fpw;
    fpw=fopen("songs/teste.mp3","wb");
    char buf[100];
    while (!feof(fp)) {
 
        fread(buf, sizeof(buf)-1, 1, fp);
        // printf("%s\n", buf);
        fwrite(buf, sizeof(buf)-1, 1, fpw);
    }
    fclose(fpw);
    fclose(fp);
}

int main(void){
    

    int udpfd, nready, maxfdp1;
    int sockfd, new_fd, keep_connection;
    fd_set rset;
    struct sockaddr_storage their_addr;
    struct sigaction sa;
    //creates a first socket and defines all its properties
    sockfd = create_socket(0);
    //listen - socket is now open for TCP connections

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    server_listen(sockfd);

    udpfd = create_socket(1);
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    FD_ZERO(&rset);
    maxfdp1 = max(sockfd, udpfd) + 1;
    // loop infinetly, everytime that receives somthing in the socket, do some operation
    while(1){
        FD_SET(sockfd, &rset);
        FD_SET(udpfd, &rset);   
        printf("waiting...\n");
        printf("udp socket %d\n", udpfd);
        printf("tcp socket %d\n", sockfd);
        printf("maxfdp1 %d\n", maxfdp1);
        if ((nready = select(maxfdp1, &rset, NULL, NULL, NULL))<0){
            if(errno = EINTR) continue;
            //else err_sys("select error");
        }

        if (FD_ISSET(sockfd, &rset)){
            new_fd = server_accept(sockfd, their_addr);
            if (new_fd == -1) continue;
            if (fork() == 0) { 
                        close(sockfd);
                        //gets our server's data
                        while (1)
                        {
                            cJSON *json = read_json();
                            keep_connection = service(new_fd, json);
                            //exit tcp connection on client call or error
                            if (keep_connection == 0) break;
                        }
                        
                        exit(0);
                    }
                    close(new_fd);
        }

        if (FD_ISSET(udpfd, &rset)) {
            printf("recebi algo\n");
            char mesg[MAXBUFLEN];
            socklen_t len = sizeof(their_addr);
            
            recvfrom(udpfd, mesg, MAXBUFLEN, 0, (struct sockaddr *)&their_addr, &len);
            char name[100] = "songs/";

            strcat(name, mesg+2);
            strcat(name, ".mp3");
            printf("%s\n", name);
            FILE *fp;
            fp=fopen(name,"rb");
            char buf[50];
            char response[MAXBUFLEN];
            int count = 0;
            while (!feof(fp)) {
                fread(buf, sizeof(buf)-1, 1, fp);
                attach_number_series_header(response, buf, count);
                memcpy(response+6, buf, sizeof(buf));
                sendto(udpfd, response, 57, 0, (struct sockaddr *)&their_addr, len);
                response[0] = '\0';             
                count++;
                usleep(10);
            }
            sendto(udpfd, "EEEEE", 6, 0, (struct sockaddr *)&their_addr, len);
            printf("sent %d datagrams\n", count);
            fclose(fp);  
        }
        
    }
    close(sockfd);
    return 0;
}