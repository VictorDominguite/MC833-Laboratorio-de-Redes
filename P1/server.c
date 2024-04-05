/*
** listener.c -- a datagram sockets "server" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "cJSON/cJSON.h"

#define MYPORT "4952"    // the port users will be connecting to

#define MAXBUFLEN 1000


cJSON* read_json_string(char* buffer){
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
    FILE *fp = fopen("example.json", "r"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n"); 
        return NULL; 
    } 

    char buffer[1024]; 
    int len = fread(buffer, 1, sizeof(buffer), fp); 
    fclose(fp); 

    return read_json_string(buffer);
}

void write_json(cJSON* json) { 

   char *json_str = cJSON_Print(json); 
  
   // write the JSON string to a file 
   FILE *fp = fopen("example.json", "w"); 
   if (fp == NULL) { 
       printf("Error: Unable to open the file.\n"); 
       return; 
   } 
   printf("%s\n", json_str); 
   fputs(json_str, fp); 
   fclose(fp); 
}
    
char* read_request(char *request,  cJSON *json){
    printf(request);
    printf("\n");
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
        cJSON* song = read_json_string(2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            id_fetched = cJSON_GetObjectItem(elem, "ID");
            if(strcmp(id_fetched->valuestring, cJSON_GetObjectItem(song, "ID")->valuestring) == 0){
                response = "0";
                break;
            }
        }
        cJSON_AddItemToArray(json, song);
        write_json(json);
        response = "1";
        break;
    case '2':
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
        response = "0";
        break;
    case '3':
        printf(request);
        printf("\n");
        strcpy(year, 2+request);
        printf(year);
        printf("\n");
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            printf("kkkkkkkkkkkkkkk\n");
            name = cJSON_GetObjectItem(elem, "Release Date");
            printf("hahahah\n");
            printf(name->valuestring);
            printf("\n");
            if(strcmp(name->valuestring, year) == 0){
                cJSON_AddItemToArray(songs, elem);
            }
        }
        printf("wow");
        response = cJSON_Print(songs);
        break;
    case '4':
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
        cJSON *genre_fetched;
        strcpy(genre, 2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            genre_fetched = cJSON_GetObjectItem(elem, "Genre");
            if(strcmp(genre_fetched->valuestring, genre) == 0){
                cJSON_AddItemToArray(songs, elem);
            }
        }
        response = cJSON_Print(json);
        break;
    case '6':
        strcpy(literal_name, 2+request);
        for (int i = 0; i < n; i++) {
            elem = cJSON_GetArrayItem(json, i);
            name_fetched = cJSON_GetObjectItem(elem, "Title");
            if(strcmp(name_fetched->valuestring, literal_name) == 0){
                response = cJSON_Print(elem);
                return response;
            }
        }
        response = "{}";
        break;
    case '7':
        response = cJSON_Print(json);
        break;
    default:
        break;
    }

    return response;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    cJSON *json = read_json();

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");
    char* response;
    while(1){
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        printf("listener: got packet from %s\n",
            inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s));
        printf("listener: packet is %d bytes long\n", numbytes);
        buf[numbytes] = '\0';

        response =  read_request(buf, json);
        if ((numbytes = sendto(sockfd, response, strlen(response), 0,
             (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("talker: sendto");
            exit(1);
        response[0] = '\0';
    }

    }
    close(sockfd);
    return 0;
}