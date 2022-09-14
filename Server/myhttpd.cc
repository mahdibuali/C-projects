#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <iostream>
#include <dlfcn.h>


void processRequest( int socket );
void handleDir(int slaveSocket, DIR *dir, char* arg, char * path, char * rpath);
char * get_arg(char *path);
void handleCGI(int fd, char *path, char *arg);
char * parentDir(char *path);
void sort(struct FileStat **files, int i ,char * arg);
int NameA(const void *a, const void * b);
int NameD(const void *a, const void * b);
int SizeA(const void *a, const void * b);
int SizeD(const void *a, const void * b);
int ModA(const void *a, const void * b);
int ModD(const void *a, const void * b);
void removeArg(char *rpath);
int QueueLength = 5;
pthread_mutex_t mutex;

struct FileStat {
  char name[100];
  char path[200];
  time_t time;
  int size;
};

typedef void (*httprunfunc)(int ssock, const char* querystring);


void sort(struct FileStat **files, int i ,char * arg) {
  if (arg == NULL) {
    return;
  } else if (arg[2] == 'N' && arg[6] == 'A') {
    qsort(files, i, sizeof(struct dirent*), NameA);
  } else if (arg[2] == 'N' && arg[6] == 'D') {
    qsort(files, i, sizeof(struct dirent*), NameD);
  } else if (arg[2] == 'M' && arg[6] == 'A') {
    qsort(files, i, sizeof(struct dirent*), ModA);
  } else if (arg[2] == 'M' && arg[6] == 'D') {
   qsort(files, i, sizeof(struct dirent*), ModD);
  } else if (arg[2] == 'S' && arg[6] == 'A') {
    qsort(files, i, sizeof(struct dirent*), SizeA);
  } else if (arg[2] == 'S' && arg[6] == 'D') {
    qsort(files, i, sizeof(struct dirent*), SizeD);
  }
  
}

int NameA(const void *a, const void *b) {
  struct FileStat * x = *(struct FileStat **) a;
  struct FileStat * y = *(struct FileStat **) b;
  return strcmp(x->name, y->name);
}
int NameD(const void *a, const void * b) {
  struct FileStat * x = *(struct FileStat **) a;
  struct FileStat * y = *(struct FileStat **) b;
  return strcmp(y->name, x->name);
}

int SizeA(const void *a, const void * b) {
  struct FileStat * x = *(struct FileStat **) a;
  struct FileStat * y = *(struct FileStat **) b;
  return x->size - y->size;
}
int SizeD(const void *a, const void * b) {
  struct FileStat * x = *(struct FileStat **) a;
  struct FileStat * y = *(struct FileStat **) b;
  return y->size - x->size;
}

int ModA(const void *a, const void * b) {
  struct FileStat * x = *(struct FileStat **) a;
  struct FileStat * y = *(struct FileStat **) b;
  return difftime(x->time, y->time);
}
int ModD(const void *a, const void * b) {
  struct FileStat * x = *(struct FileStat **) a;
  struct FileStat * y = *(struct FileStat **) b;
  return difftime(y->time, x->time);
}



char * parentDir(char *path) {
  char *p = (char *) malloc(100 * sizeof(char));
  int i = 1;
  int n =strlen(path);
  while (i < n && path[n - 1 - i] != '/') {
    i++;
  }
  int j = 0;
  while (j < n - i) {
    p[j] = path[j];
    j++;
  }
  p[j] = 0;
  return p;  
}

void removeArg(char *rpath){
  for (int i = 0; i < strlen(rpath); i++) {
    if (rpath[i] == '?') {
      rpath[i] = 0;
      break;
    }
  }
}

char * get_arg(char *path) {
  char * arg = NULL;
  for (int i = 0; i < strlen(path); i++) {
    if (path[i] == '?') {
      arg = (char  *) malloc(sizeof(char) * 1024);
      strcpy(arg, &path[i + 1]);
      path[i] = 0;
      break;
    }
  }
  return arg;
}

extern "C" void disp(int sig) {
  if (sig == SIGCHLD) {
    pid_t pid = waitpid(-1, NULL, WNOHANG);
  }
}


void processRequestThread(int socket) {
  processRequest(socket);
  shutdown(socket, SHUT_WR);
      // Close socket
  // close( socket );
}

void poolSlave(int socket) {
  while ( 1 ) {
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept( socket,
            (struct sockaddr *)&clientIPAddress,
            (socklen_t*)&alen);
    pthread_mutex_unlock(&mutex);
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    // Process request.
    processRequest( slaveSocket );
    shutdown(slaveSocket, SHUT_WR);
    // Close socket
    // close( slaveSocket );
  }
}

int main( int argc, char ** argv ) {
  int port;
  char flag = 0;
   // Add your HTTP implementation here
  if ( argc < 2 ) {
    fprintf( stderr, "%s", NULL );
    exit( -1 );
  } else if (argc == 2) {
    port = atoi( argv[1] );
  } else {
    flag = argv[1][1];
    port = atoi( argv[2] );
  }
  struct sigaction sa;
  sa.sa_handler = disp;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if(sigaction(SIGCHLD, &sa, NULL)){
    perror("sigaction");
    exit(2);
  }

  // Get the port from the arguments
  
  printf("flag=%c\n", flag);
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

  if (flag == 0) {
    while ( 1 ) {
      // Accept incoming connections
      struct sockaddr_in clientIPAddress;
      int alen = sizeof( clientIPAddress );
      int slaveSocket = accept( masterSocket,
              (struct sockaddr *)&clientIPAddress,
              (socklen_t*)&alen);
      if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
      }
      // Process request.
      processRequest( slaveSocket );
      shutdown(slaveSocket, SHUT_WR);
      // Close socket
      // close( slaveSocket );
    }
  } else if (flag == 'f') {
    while ( 1 ) {
      // Accept incoming connections
      struct sockaddr_in clientIPAddress;
      int alen = sizeof( clientIPAddress );
      int slaveSocket = accept( masterSocket,
              (struct sockaddr *)&clientIPAddress,
              (socklen_t*)&alen);
      if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
      }
      pid_t slave = fork();
      if (slave == 0) {
        // Process request.
        processRequest( slaveSocket );
        shutdown(slaveSocket, SHUT_WR);
        // close( slaveSocket );
        exit(EXIT_SUCCESS);
      }
      // Close socket
      // close( slaveSocket );
    }
  } else if (flag == 't') {
    while ( 1 ) {
      // Accept incoming connections
      struct sockaddr_in clientIPAddress;
      int alen = sizeof( clientIPAddress );
      int slaveSocket = accept( masterSocket,
              (struct sockaddr *)&clientIPAddress,
              (socklen_t*)&alen);
      if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
      }
      pthread_t thr;
      pthread_attr_t att;
      pthread_attr_init(&att);
      pthread_attr_setscope(&att, PTHREAD_SCOPE_SYSTEM);
      pthread_create(&thr, &att, (void * (*)(void *)) processRequestThread, (void *)slaveSocket); 
    }
  } else if (flag == 'p') {
    pthread_t tid[5];
    pthread_attr_t att;
    pthread_attr_init(&att);
    pthread_attr_setscope(&att, PTHREAD_SCOPE_SYSTEM);
    pthread_mutex_init(&mutex, NULL);
    for (int i = 0; i < 5; i++) {
      pthread_create(&tid[i], &att,(void *(*)(void *))poolSlave,(void *)masterSocket);
    }
    pthread_join(tid[0], NULL); 
  }

}
void handleCGI(int fd, char *path, char *arg) {

  pid_t pid = fork();
  if (pid == 0) {
    setenv("REQUEST_METHOD", "GET", 1);
    if (arg) {
      setenv("QUERY_STRING", arg, 1);
    }

    const char *l = "HTTP/1.1 200 Document follows\r\n"
						"Server: trial\r\n";

    write(fd, l, strlen(l));
    
    if (path[strlen(path) -3] == '.' && path[strlen(path) -2] == 's' && path[strlen(path) -1] == 'o') {
      printf("I entered\n");
      void * dl = dlopen(path, RTLD_LAZY);
      httprunfunc libHttpRun = (httprunfunc) dlsym(dl, "httprun");
      libHttpRun(fd, arg);
      dlclose(dl);
    } else {
      dup2(fd, 1);
      execvp(path, NULL);
      perror("execvp");
    }
    
  }
  int status = 0;
  waitpid(pid, &status, 0);

}

void handleDir(int fd, DIR *dir, char* arg, char * path, char *rpath) {
  printf("rel %s\n", rpath);
  struct FileStat **files = (struct FileStat **) malloc(100* sizeof(struct FileStat));
  struct dirent *d;
  int i = 0;
  while ((d = readdir(dir))) {
    if (d->d_name[0] != '.') {
      struct FileStat *f = (struct FileStat *) malloc(sizeof(struct FileStat));
      strcpy(f->name, d->d_name);
      removeArg(rpath);
      if (rpath[strlen(rpath) - 1] != '/') {
        strcpy(f->path, rpath);
        strcat(f->path, "/");
      } 
      strcat(f->path, f->name);
      char x[1024];
      strcpy(x, path);
      if (path[strlen(path) - 1] != '/') {
        strcat(x, "/");
      }
      strcat(x, f->name);
      struct stat s;
      stat(x, &s);
      f->time = s.st_mtime;
      if (d->d_type == DT_DIR) {
        f->size = -1;
      } else {
        f->size = s.st_size;
      }
      printf("file:%s, size %d\n", f->name, f->size);
      files[i] = f;
      i++;
    }
  }
  sort(files, i ,arg);
  // int fd = open("x.html", O_APPEND | O_RDWR);


  const char *l1 = "HTTP/1.1 200 Document follows\r\nServer: trial\r\nContent-type: text/html\r\n\r\n";
  const char *l2 = "<html>\n <head>\n  <title>Index of ";
  const char *l3 = "</title>\n </head>\n <body>\n<h1>Index of ";
  const char *l4 = "</h1>\n<table><tr>";

  const char *l5;
  if (arg == NULL) {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
              "</th><th><a href=\"?C=N;O=D\">Name</a></th><th><a href=\"?C=M;"
              "O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
              "</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
              "<tr><th colspan=\"5\"><hr></th></tr>";
  } else if (arg[2] == 'N' && arg[6] == 'A') {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
						"</th><th><a href=\"?C=N;O=D\">Name</a></th><th><a href=\"?C=M;"
						"O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
						"</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
						"<tr><th colspan=\"5\"><hr></th></tr>";
  } else if (arg[2] == 'N' && arg[6] == 'D') {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
						"</th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;"
						"O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
						"</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
						"<tr><th colspan=\"5\"><hr></th></tr>";
  } else if (arg[2] == 'M' && arg[6] == 'A') {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
						"</th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;"
						"O=D\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
						"</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
						"<tr><th colspan=\"5\"><hr></th></tr>";
  } else if (arg[2] == 'M' && arg[6] == 'D') {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
						"</th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;"
						"O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
						"</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
						"<tr><th colspan=\"5\"><hr></th></tr>";
  } else if (arg[2] == 'S' && arg[6] == 'A') {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
						"</th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;"
						"O=A\">Last modified</a></th><th><a href=\"?C=S;O=D\">Size</a>"
						"</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
						"<tr><th colspan=\"5\"><hr></th></tr>";
  } else if (arg[2] == 'S' && arg[6] == 'D') {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
						"</th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;"
						"O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
						"</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
						"<tr><th colspan=\"5\"><hr></th></tr>";
  } else {
    l5 = "<tr><th><img src=\"/icons/blue_ball.gif\" alt=\"[ICO]\">"
              "</th><th><a href=\"?C=N;O=D\">Name</a></th><th><a href=\"?C=M;"
              "O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a>"
              "</th><th><a href=\"?C=D;O=A\">Description</a></th></tr>"
              "<tr><th colspan=\"5\"><hr></th></tr>";
  }


  const char *l6 = "<tr><td valign=\"top\"><img src=\"/icons/menu.gif\" alt=\"[DIR]\"></td><td><a href=\"";

  char l7[1024];
  
  strcpy(l7, "../");
  // const char *l7 = parentDir(rpath);
  // printf("parent %s\n\n", l7);
  // const char *l7 = path;

//

  const char *l8 =  "\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>";
  write(fd, l1, strlen(l1));
  write(fd, l2, strlen(l2));
  write(fd, path, strlen(path));
  write(fd, l3, strlen(l3));
  write(fd, path, strlen(path));
  write(fd, l4, strlen(l4));
  write(fd, l5, strlen(l5));
  write(fd, l6, strlen(l6));
  write(fd, l7, strlen(l7));
  write(fd, l8, strlen(l8));
  
  const char *l9;
  for (int j = 0; j < i; j++) {
    if (files[j]->size == -1) {
      l9 = "<tr><td valign=\"top\"><img src=\"/icons/menu.gif\" alt=\"[DIR]\"></td><td><a href=\"";
    } else if (files[j]->name[strlen(files[j]->name) - 3] == 't' && files[j]->name[strlen(files[j]->name) - 2] == 'm' && files[j]->name[strlen(files[j]->name) - 1] == 'l') {
      l9 = "<tr><td valign=\"top\"><img src=\"/icons/text.gif\" alt=\"[   ]\"></td><td><a href=\"";
    } else if (files[j]->name[strlen(files[j]->name) - 3] == 'g' && files[j]->name[strlen(files[j]->name) - 2] == 'i' && files[j]->name[strlen(files[j]->name) - 1] == 'f') {
      l9 = "<tr><td valign=\"top\"><img src=\"/icons/image.gif\" alt=\"[   ]\"></td><td><a href=\"";
    } else if (files[j]->name[strlen(files[j]->name) - 3] == 's' && files[j]->name[strlen(files[j]->name) - 2] == 'v' && files[j]->name[strlen(files[j]->name) - 1] == 'g') {
      l9 = "<tr><td valign=\"top\"><img src=\"/icons/image.gif\" alt=\"[   ]\"></td><td><a href=\"";
    } else if (files[j]->name[strlen(files[j]->name) - 3] == 'x' && files[j]->name[strlen(files[j]->name) - 2] == 'b' && files[j]->name[strlen(files[j]->name) - 1] == 'm') {
      l9 = "<tr><td valign=\"top\"><img src=\"/icons/image.gif\" alt=\"[   ]\"></td><td><a href=\"";
    } else {
      l9 = "<tr><td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td><a href=\"";
    }

    const char *l10 = "</a></td><td align=\"right\">"; 
    const char *l11 = ctime(&(files[j]->time));
    const char *l12 = "</td><td align=\"right\">";
    char l13[20];
    if (files[j]->size == -1) {
      sprintf(l13, "%c",'-');
    } else {
      sprintf(l13, "%d", files[j]->size);
    }
    const char *l14 = "</td><td>&nbsp;</td></tr>\n";

    write(fd, l9, strlen(l9));
    write(fd, files[j]->path, strlen(files[j]->path));
    write(fd, "\">", strlen("\">"));
    write(fd, files[j]->name, strlen(files[j]->name));
    write(fd, l10, strlen(l10));
    write(fd, l11, strlen(l11));
    write(fd, l12, strlen(l12));
    write(fd, l13, strlen(l13));
    write(fd, l14, strlen(l14));
    free(files[j]);
  }
  const char *l15 = "<tr><th colspan=\"5\"><hr></th></tr>\n";
  const char *l16 = "</table>\n</body></html>\n";
  write(fd, l15, strlen(l15));
  write(fd, l16, strlen(l16));
  free(files);
}

void processRequest( int fd )
{
  // Buffer used to store the name received from the client
  const int MaxName = 1024;
  char name[ MaxName + 1 ];
  int nameLength = 0;
  int n;
  int doc_start = 0;
  int doc_finish = 1;
  // Currently character read
  unsigned char newChar;

  // Last character read
  unsigned char lastChar1 = 0;
  unsigned char lastChar2 = 0;
  unsigned char lastChar3 = 0;

  //
  // The client should send <name><cr><lf>
  // Read the name of the client character by character until a
  // <CR><LF> is found.
  //
  int auth = 0;
  int passLength = 0;
  char password[] = "cHl0aG9uOmlzZ3JlYXQ=\r\n"; 
  while (( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
    if (lastChar3 == '\015' && lastChar2 == '\012' && lastChar1 == '\015' && newChar == '\012' ) {
      break;
    }
    if ( nameLength == MaxName ) {
      break;
    }
    if (password[passLength] == newChar) {
      passLength++;
      if (passLength == strlen(password)) {
        auth = 1;
        break;
      }
    } else {
      passLength = 0;
    }
    
    if (doc_start && doc_finish) {
      if (newChar == ' ') {
        doc_finish = 0;
        name[nameLength] = 0;
      } else {
        name[ nameLength ] = newChar;
        nameLength++;
      }
    } else {
      if (newChar == '/') {
        doc_start = 1;
      }
    }
    lastChar3 = lastChar2;
    lastChar2 = lastChar1;
    lastChar1 = newChar;
  }
  if (auth) {
    char p[] = "/homes/mbuali/cs252/lab5-src/http-root-dir/";;
    char *path = (char *) malloc(sizeof(char) * 1024);
    strcpy(path, p);
    int cgi = 0;
    printf( "name=%s\n", name );
    if (name[0] == 'i' && name[1] == 'c' && name[2] == 'o' && name[3] == 'n' && name[4] == 's') {
      strcat(path, name);
      
    } else if (name[0] == 'c' && name[1] == 'g' && name[2] == 'i') {
      strcat(path, name);
      cgi = 1;
    } else if (name[0] == 'h' && name[1] == 't' && name[2] == 'd' && name[3] == 'o' && name[4] == 'c') {
      strcat(path, name);
    } else if (name[0] == 0) {
      char *dic_path = "htdocs/index.html";
      strcat(path, dic_path);
    } else {
      char *dic_path = "htdocs/";
      strcat(path, dic_path);
      strcat(path, name);
    }
    printf( "path=%s\n", path );
    char *arg = get_arg(path);
     
    printf( "path=%s\n", path );
    if (cgi) {
      printf("CGI\n");
      handleCGI(fd, path, arg);
      return;
    } else {
      DIR *dir = opendir(path);
      if (dir != NULL) {
        handleDir(fd, dir, arg, path, name);
        closedir(dir);
      } else {
        FILE *file = fopen(path, "r");
        if (file) {
          fseek(file, 0, SEEK_END);
          long int size = ftell(file);
          fseek(file, 0, SEEK_SET);
          char *buffer = (char *) malloc(sizeof(char) * size);
          fread(buffer, size, 1, file);
          const char * l1 = "HTTP/1.1 200 Ok\r\n";
          const char * l2 = "Server: trial\r\n";
          char l3[50];
          if (name[strlen(name) - 1] == 'g' && name[strlen(name) - 2] == 'n') {
            const char * l = "Content-type: image/png\r\n";
            strcpy(l3, l);
          } else if (name[strlen(name) - 1] == 'g' && name[strlen(name) - 2] == 'v') {
            const char * l = "Content-type: image/svg+xml\r\n";
            strcpy(l3, l);
          } else if (name[strlen(name) - 1] == 'f') {
            const char * l = "Content-type: image/gif\r\n";
            strcpy(l3, l);
          } else { 
            const char * l = "Content-type: text/html\r\n";
            strcpy(l3, l);
          }

          char l4 [50];
          sprintf(l4, "Content-Length: %ld\r\n", size);
          const char * l5 = "\r\n";
          write(fd, l1, strlen(l1));
          write(fd, l2, strlen(l2));
          write(fd, l3, strlen(l3));
          write(fd, l4, strlen(l4));
          write(fd, l5, strlen(l5));
          for (int i = 0; i < size; i++) {
            write(fd, &buffer[i], 1);
          }
          free(buffer);
          fclose(file);
        }
      }
    }
  } else {
    const char * l1 = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"myhttpd-cs252\"\r\n\r\n";
    write(fd, l1, strlen(l1));
  }
  
}
