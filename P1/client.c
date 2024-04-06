/*
** talker.c -- a datagram "client" demo
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

#define SERVERPORT "4952"    // the port users will be connecting to
#define MAXBUFLEN 1000
#define HOSTNAME "tobias-desktop"


int service(char *buf)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(HOSTNAME, SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        printf("addr: %s\n", p->ai_addr->sa_data);
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    printf("sendto didnt fail!!!!!!!\nnum bytes: %d\n", numbytes);
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        p->ai_addr, &p->ai_addrlen)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    printf("received nicely\n");
    buf[numbytes] = '\0';
    printf("%s\n", buf);

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}


int main(){
    printf(
        "Welcome!\n\n"
        "What would you like to do?\n"
        "\t1. Register a new song\n"
        "\t2. Remove a song\n"
        "\t3. List all songs from a year\n"
        "\t4. List all songs of a certain language from a certain year\n"
        "\t5. List all songs of a genre\n"
        "\t6. List all info from a certain song\n"
        "\t7. List all the information from all songs\n"
    );

    char option;
    printf("\nType the number corresponding to the desired option: ");
    scanf(" %c", &option);

    char buf[MAXBUFLEN] = {option, '/', '\0'};

    char id[5];
    char title[100];
    char artist[100];
    char language[100];
    char genre[100];
    char chorus[100];
    char year[5];

    switch (option)
    {
    case '1':
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
        printf("Type the year the song was released: ");
        scanf("\n%[^\n]%*c", year);

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
        printf("%s\n", buf);

        service(buf);
        break;
    
    case '2':
        printf("Type the ID: ");
        scanf(" %s", id);
        strcat(buf, id);

        service(buf);
        break;
        
    case '3':
        printf("Type the desired year: ");
        scanf(" %s", year);
        strcat(buf, year);

        service(buf);
        break;

    case '4':
        printf("Type the desired year: ");
        scanf("%s", year);
        strcat(buf, year);
        strcat(buf, "/");

        printf("Type the desired language: ");
        scanf("%s", language);
        strcat(buf, language);

        service(buf);
        break;
    
    case '5':
        char genre[MAXBUFLEN];
        printf("Type the desired genre: ");
        scanf("%s", genre);
        strcat(buf, genre);

        service(buf);
        break;
    
    case '6':
        printf("Type the name of the song: ");
        scanf("\n%[^\n]%*c", title);
        strcat(buf, title);

        service(buf);
        break;
    
    case '7':
        service(buf);
        break;

    default:
        break;
    }

    return 0;
}