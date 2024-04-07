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

#define MYPORT "4952"    // the port users will be connecting to

#define MAXBUFLEN 100000
#define BACKLOG 10   // how many pending connections queue will hold

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
    char genre[MAXBUFLEN];
    char year[MAXBUFLEN];
    char literal_name[MAXBUFLEN];
    char* response;
    int n = cJSON_GetArraySize(json);
    switch (request[0])
    {
    case '1':
        //1. Register a new song
        // returns 0/1 if failed/succeded
        cJSON* song = read_json_string(2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            id_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(id_fetched->valuestring, cJSON_GetObjectItem(song, "ID")->valuestring) == 0){
                response = "0"; //cannot register a song with an existing ID
                return response;
            }
        }
        cJSON_AddItemToArray(json, song);
        write_json(json);
        response = "1";
        break;
    case '2':
        //2. Delete a song
        // returns 0/1 if failed/succeded
        char id[MAXBUFLEN];
        strcpy(id, 2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            id_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(id_fetched->valuestring, id) == 0){
                cJSON_DeleteItemFromArray(json,i);
                write_json(json);
                response = "1";
                return response;
            }
        }
        response = "0"; //ID doesnt exist
        break;
    case '3':
        //3. List all songs from a year
        // returns [song_object, ...] being song_object each of the songs from the year requested 
        strcpy(year, 2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name = cJSON_GetObjectItem(elem, "Release Date");
            if(strcmp(name->valuestring, year) == 0){
                cJSON_AddItemToArray(songs, elem);
            }
        }
        response = cJSON_Print(songs);
        break;
    case '4':
        //4. List all songs of a certain language from a certain year
        // returns [song_object, ...] being song_object each of the songs from the year and language requested  
        char language[MAXBUFLEN];
        cJSON *language_fetched;
        strcpy(language, 7+request);
        strncpy(year, 2+request, 4);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name = cJSON_GetObjectItem(elem, "Release Date");
            language_fetched = cJSON_GetObjectItem(elem, "Language");
            if(strcmp(name->valuestring, year) == 0 && strcmp(language_fetched->valuestring, language) == 0){
                cJSON_AddItemToArray(songs, elem);
            }
        }
        response = cJSON_Print(songs);
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
                cJSON_AddItemToArray(songs, elem);
            }
        }
        response = cJSON_Print(songs);
        break;
    case '6':
        //6. List all info from a certain song
        // returns [json object]/[] if the ID exists/does not exist
        strcpy(literal_name, 2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(name_fetched->valuestring, literal_name) == 0){
                cJSON_AddItemToArray(songs, elem);
                break;
            }
        }
        response = cJSON_Print(songs);
        break;
    case '7':
        //7. List all the information from all songs
        // returns [song_object, ...] being song_object each of the songs from database 
        response = cJSON_Print(json);
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

int main(void)
{
int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int numbytes;
    char buf[MAXBUFLEN];


    cJSON *json = read_json();  

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    char* response;
    // loop infinetly, everytime that receives somthing in the socket, do some operation
    while(1){
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (numbytes = recv(new_fd, buf, MAXBUFLEN-1 , 0) == -1)
            perror("send");

        buf[numbytes] = '\0';
        printf("%s\n", buf);
        response =  read_request(buf, json);// act!
        // respond properly to the request:
        if (numbytes = send(new_fd, response, strlen(response), 0) == -1) {
            perror("send");
            exit(1);
        response[0] = '\0';



        close(new_fd);
 // parent doesn't need this
    }

    }
    close(sockfd);
    return 0;
}