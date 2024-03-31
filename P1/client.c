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
#define MAXBUFLEN 100
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

    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        p->ai_addr, &p->ai_addrlen)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    buf[numbytes] = '\0';
    printf("%s\n", buf);

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}


int main(){
    printf(
        "Welcome to Tobify!\n\n"
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

    switch (option)
    {
    case '1':
        break;
    
    case '2':
        break;
        
    case '3':
        printf("Boa escolha tigrao!\n");
        char year[5];
        printf("Type the desired year: ");
        scanf("%s", year);
        strcat(buf, year);
        strcat(buf, "/\0");
        service(buf);
        break;

    case '4':
        break;
    
    case '5':
        break;
    
    case '6':
        break;
    
    case '7':
        break;

    default:
        break;
    }

    return 0;
}