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
#include <signal.h>
#include <netdb.h>
#include "cJSON/cJSON.h"

#define MYPORT "3220"    // the port users will be connecting to

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

    char buffer[1024]; 
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

    switch (request[0])
    {
    case '0':
        strcpy(response, "0");
        break;
    case '1':
        //1. Register a new song
        // returns 0/1 if failed/succeded
        cJSON* song = read_json_string(2+request);
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
        strcpy(id, 2+request);
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
        strcpy(year, 2+request);
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
        strcpy(language, 7+request);
        strncpy(year, 2+request, 4);
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
        strcpy(genre, 2+request);
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
        strcpy(literal_name, 2+request);
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

int create_socket(){
    /*
    Creates a new socket and binds it based on server's IP and port determined

    */
    int rv, sockfd;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 
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

char* padding(char* final_string, char* s, int size){
    int pad = size - strlen(s);
    strcpy(final_string, "\0");
    char* pad_item = "0";
    for(int i = 0; i<pad; i++){
        strcat(final_string, pad_item);
    }
    strcat(final_string, s);
}

char* attach_buf_size_header(char* buf){
    char final_string[MAXBUFLEN];
    int size = strlen(buf) + HEADERBUFSIZELEN + 1;
    char size_str[HEADERBUFSIZELEN];
    sprintf(size_str, "%d", size);  
    padding(final_string, size_str, HEADERBUFSIZELEN);
    strcat(final_string, buf);
    strcpy(buf, final_string);
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

    // Get the total bytes that are going to be received
    do {
        if ((numbytes_read = read(sockfd, partial_response, HEADERBUFSIZELEN)) == -1) {
            perror("read");
            return -1;
        }
        total_received += numbytes_read;
        partial_response[numbytes_read] = '\0';
        strcat(response, partial_response);
        response[total_received] = '\0';

    } while(total_received < HEADERBUFSIZELEN);

    strncpy(str_total_to_receive, response, HEADERBUFSIZELEN);
    str_total_to_receive[HEADERBUFSIZELEN] = '\0';
    total_to_receive = atoi(response);
    total_to_receive -= total_received;

    // Receives the rest of the message
    while(total_received < total_to_receive) {
        if ((numbytes_read = read(sockfd, response, MAXBUFLEN-1)) == -1) {
            perror("read");
            return -1;
        }
        total_received += numbytes_read;
        total_to_receive -= numbytes_read;
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
    response = read_request(&buf[HEADERBUFSIZELEN + 1], json);// act!
    
    attach_buf_size_header(response);
    send_all(new_fd, response);

    free(response);
    if (buf[0] == '0') return 0; // terminate connection
    return 1;
}

int main(void){
    int sockfd, new_fd, keep_connection;
    struct sockaddr_storage their_addr;
    //creates a first socket and defines all its properties
    sockfd = create_socket();
    //listen - socket is now open for TCP connections
    server_listen(sockfd);
    // loop infinetly, everytime that receives somthing in the socket, do some operation
    while(1){
        //accept new_connection
        new_fd = server_accept(sockfd, their_addr);
        if (new_fd == -1) continue;
        // resolve new_connection in child porcess
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
    close(sockfd);
    return 0;
}