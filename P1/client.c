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
#include <assert.h>
#include "cJSON/cJSON.h"

#define SERVERPORT "4952"    // the port users will be connecting to
#define HOSTNAME "DESKTOP-32IVUBT"
#define MAXBUFLEN 10000
#define MAXIDLEN 5
#define MAXYEARLEN 5
#define MAXSUBSECTIONLEN 50
#define MAXCHORUSLEN 200

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

/* Stablish a TCP connection with the server, sends an operation request 
 * to it, then receives and prints the server response */
int service(char *buf) {
    int sockfd;  
    struct addrinfo hints, *servinfo;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char* response = (char*)malloc(MAXBUFLEN);
    int numbytes_read;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(HOSTNAME, SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
            servinfo->ai_protocol)) == -1) {
        perror("client: socket");
    }

    if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(sockfd);
        perror("client: connect");
    }

    inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr *)servinfo->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    // Sends operation request to server
    if (write(sockfd, buf, strlen(buf)) == -1) {
        perror("send");
        exit(1);
    }
    // Receives server response
    if ((numbytes_read = read(sockfd, response, MAXBUFLEN-1)) == -1) {
        perror("recv");
        exit(1);
    }
    response[numbytes_read] = '\0';
    print_query_results(response);

    free(response);
    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}

/* Given an operation code as a char, process the corresponding request */
void process_operation(char option) {
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
        service(buf);
        break;
    
    case '2': // Remove song

        // Asking the user for the ID
        printf("Type the ID: ");
        scanf(" %s", id);
        strcat(buf, id);

        service(buf);
        break;
        
    case '3': // Search by year
        printf("Type the desired year: ");
        scanf(" %s", year);
        strcat(buf, year);

        service(buf);
        break;

    case '4': // Search by year and language
        printf("Type the desired year: ");
        scanf("%s", year);
        strcat(buf, year);
        strcat(buf, "/");

        printf("Type the desired language: ");
        scanf("%s", language);
        strcat(buf, language);

        service(buf);
        break;
    
    case '5': // Search by genre
        char genre[MAXBUFLEN];
        printf("Type the desired genre: ");
        scanf("%s", genre);
        strcat(buf, genre);

        service(buf);
        break;
    
    case '6': // Search by song id
        printf("Type the ID of the song: ");
        scanf("%s", id);
        strcat(buf, id);

        service(buf);
        break;
    
    case '7': // Display information from all songs
        service(buf);
        break;

    default:
        printf("\nInvalid operation with code \'%c\'.\n\n", option);
        break;
    }
}

int main() {
    char option;

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

        if (option == '0') {
            printf("\nExiting program.\n");
            return 0;
        }
        process_operation(option);
    }

    return 0;
}