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
#include <sys/time.h>
#include <sys/stat.h>
#include <ctype.h>
#include "cJSON/cJSON.h"


#define SERVERPORT "3221"    // the port users will be connecting to
#define MAXBUFLEN 10000
#define MAXIDLEN 5
#define MAXYEARLEN 5
#define MAXSUBSECTIONLEN 50
#define MAXCHORUSLEN 200
#define HEADERBUFSIZELEN 5
#define TIMEOUT_SECONDS 3
#define MAXCHUNKS 100000
#define PACKAGESIZE 57
#define UDPHEADERSIZE 8

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

int send_all(int sockfd, char* buf) {
    int total_sent = 0, n;
    int bytes_to_send = strlen(buf);
    int total_to_send = strlen(buf);
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


/* Sends an UDP message to the server requesting to download a song */
int request_download(char *buf, char *hostname) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(hostname, SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        // if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        //     close(sockfd);
        //     perror("listener: bind");
        //     continue;
        // }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to create socket\n");
        return -2;
    }

    if (sockfd < 0) return -1;

    // Sends the download request to the server via UDP
    if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
            p->ai_addr, p->ai_addrlen)) == -1) {
        perror("client: sendto");
        exit(1);
    }

    printf("Sent download request to server!\n");

    freeaddrinfo(servinfo);

    return sockfd;
}

/* Function to determine which song chunk comes first */
int comp(const void * elem1, const void * elem2) {
    int i, j;
    char num1_char[10], num2_char[10];
    int num1, num2;
    char *e1 = (char *)elem1, *e2 = (char *)elem2;

    for (i = 0; e1[i] != '/'; i++);
    for (j = 0; e2[j] != '/'; j++);

    strncpy(num1_char, (char *)elem1, i);
    strncpy(num2_char, (char *)elem2, j);

    num1 = atoi(num1_char);
    num2 = atoi(num2_char);

    if (num1 > num2) return  1;
    if (num1 < num2) return -1;
    return 0;
}

/* Copies numbytes from source to destination */
void copy_buffer(char *source, char *destination, int numbytes) {
    for (int i = 0; i < numbytes; i++) {
        destination[i] = source[i];
    }
}

/* Returns 1 if str is a number or 0 otherwise */
int isnumerical(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        if (!isdigit(str[i])) {
            return 0;
        }
    }
    return 1;
}

/* Receives the requested song through UDP and saves it in 
 * a file */
int download_song(int sockfd, char* song_id) {
    // This code was mainly based on the code from https://beej.us/guide/bgnet/html/
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    fd_set fds;
    int n;
    struct timeval tv;
    char song_chunk[60];
    char song_chunks[80000][60];
    char fsize_str[20];
    char file_path[100];
    int fsize;

    if (sockfd < 0) return 0;

    struct stat st = {0};

    if (stat("./downloads", &st) == -1) {
        mkdir("./downloads", 0700);
    }

    sprintf(file_path, "downloads/%s.mp3", song_id);
    FILE *fp;
    fp = fopen(file_path,"wb");

    // set up the file descriptor set
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);


    printf("Downloading song...\n");
    
    int total_bytes = 0, dgrams_received = 0;

    addr_len = sizeof their_addr;

    // Expects to receive file size
    if ((numbytes = recvfrom(sockfd, fsize_str, sizeof(fsize_str)-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
    }

    fsize_str[numbytes] = '\0';
    
    if (!isnumerical(fsize_str)) {
        printf("Error getting file size. Please, try again.\n");
        return -1;
    }

    fsize = atoi(fsize_str);
    
    // Receives the song's content
    while(total_bytes < fsize) {
        // set up the struct timeval for the timeout
        tv.tv_sec = TIMEOUT_SECONDS;
        tv.tv_usec = 0;
        // wait until timeout or data received
        n = select(sockfd+1, &fds, NULL, NULL, &tv);
        if (n == 0) {
            printf("Timeout while downloading\n");
            break;
        } // timeout
        if (n == -1) return -1; // error

        // receives song from server using UDP
        if ((numbytes = recvfrom(sockfd, song_chunk, sizeof(song_chunk)-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        
        song_chunk[numbytes] = '\0';
        total_bytes += numbytes - UDPHEADERSIZE;

        memcpy(song_chunks[dgrams_received], song_chunk, numbytes);

        dgrams_received += 1;
    }
    printf("\nReceived %d datagrams\n", dgrams_received);

    qsort(song_chunks, dgrams_received, sizeof(*song_chunks), comp);
    int flag = 0;
    for(int i = 0; i < dgrams_received; i++) {

        flag = fwrite(song_chunks[i]+UDPHEADERSIZE-2, 1, PACKAGESIZE-UDPHEADERSIZE, fp);
        if (!flag) printf("Error writing file\n");

    }

    printf("\nSong downloaded! Got %d bytes. Saved to %s.\n\n", total_bytes, file_path);
    
    fclose(fp);
    close(sockfd);
    return total_bytes;
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

    print_query_results(response+HEADERBUFSIZELEN+1);

    free(response);

    return 0;
}

/* Given an operation code as a char, process the corresponding request */
void process_operation(char option, int sockfd, char *hostname) {
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
        scanf("\n%[^\n]%*c", genre);
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
    
    case '8': // Download a song
        printf("Type the ID of the song: ");
        scanf("%s", id);
        strcat(buf, id);
        int udp_socket = request_download(buf, hostname);
        download_song(udp_socket, id);
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
            "\t8. Download a song\n"
            "\t0. Exit program\n"
        );

        printf("\nType the number corresponding to the desired option: ");
        scanf(" %c", &option);

        process_operation(option, *sockfd, argv[1]);
        
        if (option == '0') {
            close_tcp_connection(servinfo, *sockfd);
            free(sockfd);
            printf("\nExiting program.\n");
            return 0;
        }
    }

    return 0;
}