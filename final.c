
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>


#define PORT 2601
#define N_MAX_CLIENTS 5
#define MAXBUF 4096

#define true 1
#define false 0
#define GMT_TIME "%a, %d %b %Y %H:%M:%S GMT"
typedef int bool;

typedef struct HTTP_Server {
    int socket;
    int port;
} HTTP_Server;

typedef struct Route {
    char* key;
    char* value;
    struct Route *left, *right;
} Route;

typedef struct Host {
    int client_socket;
    struct sockaddr_in address;
    Route* route;
} Host;




void init_server(HTTP_Server* http_server, int port)
{
    http_server->port = port;

    // Criação do socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        printf("Não foi possível criar o socket\n");
        return 1;
    }
    // Configuração do endereço do servidor
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port); // Número da porta do servidor
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Vinculação do socket a um endereço e porta
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Não foi possível vincular o socket\n");
        return 1;
    }

    // Espera por conexões de clientes
    listen(server_socket,N_MAX_CLIENTS);
    printf("Aguardando por conexões...\n");
    
    http_server->socket = server_socket;
    printf("Servidor HTTP inicializado\nPorta: %d\n", http_server->port);
}

// Inicializa uma estrutura para uma árvore binária de busca
Route * initRoute(const char* key, const char* value) {
    Route* temp = (Route*) malloc(sizeof(Route));
    temp->key = strdup(key);
    temp->value = strdup(value);
    temp->left = temp->right = NULL;
    return temp;
}

// Printa as rotas disponiveis
void inorder(Route* root) {
    if (root != NULL) {
        inorder(root->left);
        printf("%s -> %s \n", root->key, root->value);
        inorder(root->right);
    }
}

// Adciona uma rota nova
Route* addRoute(Route* root, const char* key, const char* value) {
    if (root == NULL) {
        return initRoute(key, value);
    }

    if (strcmp(key, root->key) == 0) {
        printf("============ ATENÇÃO CARA ============\n");
        printf("A rota para \"%s\" já existe\n", key);
    } else if (strcmp(key, root->key) > 0) {
        root->right = addRoute(root->right, key, value);
    } else {
        root->left = addRoute(root->left, key, value);
    }

    return root;
}

// Busca na árvore binária
Route* search(Route* root, const char* key) {
    if (root == NULL) {
        return NULL;
    }

    if (strcmp(key, root->key) == 0) {
        return root;
    } else if (strcmp(key, root->key) > 0) {
        return search(root->right, key);
    } else if (strcmp(key, root->key) < 0) {
        return search(root->left, key);
    }

    return NULL;
}

// Retorna o tipo do arquivo
const char * get_tipo(const char * name) {
    const char * ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

// Envia os dados para a página web 
void sendData(int clientfd, const char* filename){
    char buffer;
    int fd;
    fd = open(filename, O_RDONLY);

    while(read(fd, &buffer, 1)){
        write(clientfd, &buffer, 1);
    }
    close(fd);
}

// Envia o cabeçalho
void sendHeader(int clientfd, int status, char* title, char* tipo, int length, char* protocol){
    char header[MAXBUF] = {0};
    char aux[200] = {0};
    char timebuf[150] = {0};
    time_t now;

    sprintf(aux, "%s %d %s\r\n", protocol, status, title);
    strcat(header, aux);

    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), GMT_TIME, gmtime(&now));
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "Date: %s\r\n", timebuf);
    strcat(header, aux);


    if (tipo) {
        memset(aux, '\0', sizeof(aux));
        sprintf(aux, "Content-Type: %s\r\n", tipo);
        strcat(header, aux);
    }
    if (length >= 0) {
        memset(aux, '\0', sizeof(aux));
        sprintf(aux, "Content-Length: %d\r\n", length);
        strcat(header, aux);
    }
    strcat(header, "Connection: close\r\n\r\n");
    
    printf("%s", header);
    send(clientfd, header, strlen(header), 0);
}

// Função usada para tratar uma conexão de cliente em um servidor web. 
void* handle_client(void* arg) {
    Host* final = (Host*) arg;
    int client_socket = final->client_socket;
    Route* route = final->route;
    struct stat statbuff;
    char tipo[50] ={0};  
     
    free(arg);

    char client_msg[4096] = "";

    read(client_socket, client_msg, 4095);
    printf("%s\n", client_msg);

    char *method = "";
    char *urlRoute = "";

    char *client_http_header = strtok(client_msg, "\n");

    printf("\n\n%s\n\n", client_http_header);

    char *header_token = strtok(client_http_header, " ");

    int header_parse_counter = 0;

    while (header_token != NULL) {

        switch (header_parse_counter) {
            case 0:
                method = header_token;
                break;
            case 1:
                urlRoute = header_token;
                break;
        }
        header_token = strtok(NULL, " ");
        header_parse_counter++;
    }

    printf("The method is %s\n", method);
    printf("The route is %s\n", urlRoute);

    char template[100] = "";

    if (strstr(urlRoute, "/static/") != NULL) {
        strcat(template, "static/index.css");
    } else {
        Route* destination = search(route, urlRoute);
        strcat(template, "templates/");

        if (destination == NULL) {
            strcat(template, "404.html");
        } else {
            strcat(template, destination->value);
        }
    }
    if(stat(template, &statbuff) < 0){
        perror("404 NOT FOUND\n");
        return NULL;
    }
    size_t length = S_ISREG(statbuff.st_mode) ? statbuff.st_size : -1;
    
    
    sendHeader(client_socket, 200, "OK", (char*)get_tipo(template), length, "HTTP/1.1");

    // char http_header[4096] = "HTTP/1.1 200 OK\r\n\r\n";

    // strcat(http_header, response_data);
    // strcat(http_header, "\r\n\r\n");

    // send(client_socket, http_header, strlen(http_header), 0);

    // char *response_data = render_static_file(template);
    sendData(client_socket, template);
    close(client_socket);
    // free(response_data);
    pthread_exit(NULL);
}


int main() {
    HTTP_Server http_server;
    init_server(&http_server, PORT);

    Route* route = initRoute("/", "index.html");
    addRoute(route, "/teste", "teste.html");
    addRoute(route, "/ferias.webp", "ferias.webp");

    printf("\n====================================\n");
    printf("=========Todas as Rotas disponíveis========\n");
    inorder(route);

    while (1) {

        int client_socket;
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        client_socket = accept(http_server.socket, (struct sockaddr *) &client_address, &client_address_len);

        Host* final = (Host*) malloc(sizeof(Host));
        final->client_socket = client_socket;
        final->route = route;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void*) final);
        pthread_detach(tid);
    }

    return 0;
}
