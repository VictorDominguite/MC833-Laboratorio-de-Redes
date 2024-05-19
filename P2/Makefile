all: server.c client.c cJSON/cJSON.c
	gcc -Wall -Werror -o server.exe server.c cJSON/cJSON.c
	gcc -Wall -Werror -o client.exe client.c cJSON/cJSON.c

client: client.c cJSON/cJSON.c
	gcc -Wall -Werror -o client.exe client.c cJSON/cJSON.c

server: server.c cJSON/cJSON.c
	gcc -Wall -Werror -o server.exe server.c cJSON/cJSON.c