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

#define SERVERPORT "3220"    // the port users will be connecting to
#define MAXBUFLEN 10000
#define MAXIDLEN 5
#define MAXYEARLEN 5
#define MAXSUBSECTIONLEN 50
#define MAXCHORUSLEN 200
#define HEADERBUFSIZELEN 5

/* Receives a string corresponding to a json file and converts it to 
 * a cJSON object. The converted json object is returned */
cJSON* read_json_string(char* buffer) {
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

/* Given a buffer (string) containing the response of the server to a query,
 * formats and prints the information */
void print_query_results(char *buf) {
    printf("\n");
    // if the response is of size 1, then it's a number indicating if
    // the operation was successful or not (for adding/removing songs)
    if ((int) strlen(buf) == 1) {
        if (buf[0] == '1') 
            printf("Operation succeeded!\n\n");
        else
            printf("Operation failed.\n\n");
        return;
    }

    // else, the response is in a json format, so the string is converted
    // to a cJSON object
    cJSON *json = read_json_string(buf);
    int n = cJSON_GetArraySize(json);
    
    if (n == 0) {
        printf("No results.\n\n");
        return;
    }

    cJSON *elem;

    cJSON *id;
    cJSON *title;
    cJSON *artist;
    cJSON *language;
    cJSON *genre;
    cJSON *chorus;
    cJSON *year;

    // Iterates over the elements in the response array, gets the information
    // from its fields and prints them
    for (int i = 0; i < n; i++) {
        elem = cJSON_GetArrayItem(json, i);
        
        id = cJSON_GetObjectItem(elem, "ID");
        title = cJSON_GetObjectItem(elem, "Title");
        artist = cJSON_GetObjectItem(elem, "Artist");
        language = cJSON_GetObjectItem(elem, "Language");
        genre = cJSON_GetObjectItem(elem, "Genre");
        chorus = cJSON_GetObjectItem(elem, "Chorus");
        year = cJSON_GetObjectItem(elem, "Release Date");

        printf(
            "ID: %s\n"
            "Title: %s\n"
            "Artist: %s\n"
            "Language: %s\n"
            "Genre: %s\n"
            "Chorus: \"%s\"\n"
            "Release Date: %s\n\n", 
            id->valuestring,
            title->valuestring,
            artist->valuestring,
            language->valuestring,
            genre->valuestring,
            chorus->valuestring,
            year->valuestring
            );
    }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
    strcat(final_string, "/");
    strcat(final_string, buf);
    strcpy(buf, final_string);
}

int send_all(int sockfd, char* buf) {
    int total_sent = 0, n;
    int bytes_to_send = strlen(buf);
    int total_to_send = strlen(buf);
    printf("%s\n", buf);
    while(total_sent < total_to_send){
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
    
    // Convert the number from string to int
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


/* Sends an operation request through a TCP connection using the
 * socket sockfd, then receives and prints the server response */
int service(char *buf, int sockfd) {
    char* response = (char*)malloc(MAXBUFLEN);

    attach_buf_size_header(buf);
    // Sends operation request to server
    if (send_all(sockfd, buf) == -1) {
        perror("send_all");
        exit(1);
    }
    
    // Receives server response
    if (read_all(sockfd, response) == -1) {
        perror("read_all");
        exit(1);
    }
    
    print_query_results(response);

    free(response);

    return 0;
}

/* Given an operation code as a char, process the corresponding request */
void process_operation(char option, int sockfd) {
    char buf[MAXBUFLEN] = {option, '/', '\0'};
    char id[MAXIDLEN];
    char title[MAXSUBSECTIONLEN];
    char artist[MAXSUBSECTIONLEN];
    char language[MAXSUBSECTIONLEN];
    char genre[MAXSUBSECTIONLEN];
    char chorus[MAXCHORUSLEN];
    char year[MAXYEARLEN];

    switch (option)
    {
    case '0': // Ends connection
        service(buf, sockfd);
        break;

    case '1': // Add new song

        // Getting new entry info from user
        printf("Type the ID: ");
        scanf("\n%[^\n]%*c", id);
        printf("Type the song title: ");
        scanf("\n%[^\n]%*c", title);
        printf("Type the artist's name: ");
        scanf("\n%[^\n]%*c", artist);
        printf("Type the language of the song: ");
        scanf("\n%[^\n]%*c", language);
        printf("Type the genre: ");
        scanf("\n%[^\n]%*c", genre);
        printf("Type the chorus: ");
        scanf("\n%[^\n]%*c", chorus);
        do {
        printf("Type the year the song was released (4 digits): ");
        scanf("\n%[^\n]%*c", year);
        } while(strlen(year) != 4);
        // Arranging buffer with json formatted data for the new song
        char new_song_json[MAXBUFLEN];
        snprintf(new_song_json, sizeof new_song_json, 
        "{"
            "\"ID\": \"%s\","
            "\"Title\": \"%s\","
            "\"Artist\": \"%s\","
            "\"Language\": \"%s\","
            "\"Genre\": \"%s\","
            "\"Chorus\": \"%s\","
            "\"Release Date\": \"%s\""
        "}", 
        id, title, artist, language, genre, chorus, year);
        strcat(buf, new_song_json);

        // Sends request to the server and (hopefully) gets a response
        service(buf, sockfd);
        break;
    
    case '2': // Remove song

        // Asking the user for the ID
        printf("Type the ID: ");
        scanf(" %s", id);
        strcat(buf, id);

        service(buf, sockfd);
        break;
        
    case '3': // Search by year
        printf("Type the desired year: ");
        scanf(" %s", year);
        strcat(buf, year);

        service(buf, sockfd);
        break;

    case '4': // Search by year and language
        printf("Type the desired year: ");
        scanf("%s", year);
        strcat(buf, year);
        strcat(buf, "/");

        printf("Type the desired language: ");
        scanf("%s", language);
        strcat(buf, language);

        service(buf, sockfd);
        break;
    
    case '5': // Search by genre
        char genre[MAXBUFLEN];
        printf("Type the desired genre: ");
        scanf("%s", genre);
        strcat(buf, genre);

        service(buf, sockfd);
        break;
    
    case '6': // Search by song id
        printf("Type the ID of the song: ");
        scanf("%s", id);
        strcat(buf, id);

        service(buf, sockfd);
        break;
    
    case '7': // Display information from all songs
        service(buf, sockfd);
        break;

    default:
        printf("\nInvalid operation with code \'%c\'.\n\n", option);
        break;
    }
}

/* Connects to the host given by hostname using a TCP connection, saves
 * the created socket in sockfd and returns an addrinfo struct */
struct addrinfo* stablish_tcp_connection(char *hostname, int *sockfd) {
    struct addrinfo hints, *servinfo;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    if ((*sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
            servinfo->ai_protocol)) == -1) {
        perror("client: socket");
    }

    if (connect(*sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(*sockfd);
        perror("client: connect");
    }

    inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr *)servinfo->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    return servinfo;
}

/* Closes a TCP connection with the information from servinfo and sockfd */
void close_tcp_connection(struct addrinfo *servinfo, int sockfd) {
    freeaddrinfo(servinfo);
    close(sockfd);
}

int main(int argc, char *argv[]) {
    char option;
    int *sockfd = (int*) malloc(sizeof(int));
    struct addrinfo *servinfo;

    if (argc != 2) {
        free(sockfd);
        fprintf(stderr,"usage: %s hostname\n", argv[0]);
        exit(1);
    }
    
    servinfo = stablish_tcp_connection(argv[1], sockfd);

    if (servinfo == NULL) {
        free(sockfd);
        printf("Failed to stablish connection. Exiting program\n");
        exit(1);
    }

    printf(
        "Welcome!\n\n"
    );

    // Runs requests for operations until the user exits the program
    while(1) {
        printf(
            "What would you like to do?\n"
            "\t1. Register a new song\n"
            "\t2. Remove a song\n"
            "\t3. List all songs from a year\n"
            "\t4. List all songs of a certain language from a certain year\n"
            "\t5. List all songs of a genre\n"
            "\t6. List all info from a certain song\n"
            "\t7. List all the information from all songs\n"
            "\t0. Exit program\n"
        );

        printf("\nType the number corresponding to the desired option: ");
        scanf(" %c", &option);

        process_operation(option, *sockfd);
        
        if (option == '0') {
            close_tcp_connection(servinfo, *sockfd);
            free(sockfd);
            printf("\nExiting program.\n");
            return 0;
        }
    }

    return 0;
}