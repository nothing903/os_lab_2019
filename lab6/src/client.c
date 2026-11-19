#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  /* <--- Вот он, добавлен сюда */
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

struct Server {
  char ip[255];
  int port;
};

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (result + a) % mod;
    a = (a * 2) % mod;
    b /= 2;
  }
  return result % mod;
}

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  errno = 0;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }
  if (errno != 0)
    return false;
  *val = i;
  return true;
}

struct ClientTask {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  bool ok;
};

static void *ServerWorker(void *arg) {
  struct ClientTask *t = (struct ClientTask *)arg;
  t->ok = false;

  struct hostent *hostname = gethostbyname(t->server.ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", t->server.ip);
    return NULL;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(t->server.port);
  server.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

#ifdef _WIN32
  SOCKET sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck == INVALID_SOCKET) {
#else
  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
#endif
    fprintf(stderr, "Socket creation failed!\n");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Connection failed to %s:%d\n", t->server.ip, t->server.port);
#ifdef _WIN32
    closesocket(sck);
#else
    close(sck);
#endif
    return NULL;
  }

  char task[sizeof(uint64_t) * 3];
  memcpy(task, &t->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &t->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &t->mod, sizeof(uint64_t));

  if (send(sck, task, sizeof(task), 0) < 0) {
    fprintf(stderr, "Send failed\n");
#ifdef _WIN32
    closesocket(sck);
#else
    close(sck);
#endif
    return NULL;
  }

  char response[sizeof(uint64_t)];
  int got = recv(sck, response, sizeof(response), 0);
  if (got != sizeof(response)) {
    fprintf(stderr, "Receive failed\n");
#ifdef _WIN32
    closesocket(sck);
#else
    close(sck);
#endif
    return NULL;
  }

  memcpy(&t->result, response, sizeof(uint64_t));
  t->ok = true;

#ifdef _WIN32
  closesocket(sck);
#else
  close(sck);
#endif
  return NULL;
}

static struct Server *ReadServers(const char *path, unsigned int *count) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "Can not open servers file: %s\n", path);
    return NULL;
  }

  struct Server *servers = NULL;
  unsigned int n = 0, cap = 0;
  char line[512];

  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0')
      continue;

    char *colon = strrchr(line, ':');
    if (!colon) {
      fprintf(stderr, "Bad server line: %s\n", line);
      continue;
    }
    *colon = '\0';

    if (n == cap) {
      cap = cap ? cap * 2 : 4;
      struct Server *tmp = realloc(servers, cap * sizeof(struct Server));
      if (!tmp) {
        free(servers);
        fclose(f);
        return NULL;
      }
      servers = tmp;
    }

    memset(&servers[n], 0, sizeof(struct Server));
    strncpy(servers[n].ip, line, sizeof(servers[n].ip) - 1);
    servers[n].port = atoi(colon + 1);
    n++;
  }

  fclose(f);
  *count = n;
  return servers;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  uint64_t k = (uint64_t)-1;
  uint64_t mod = (uint64_t)-1;
  char servers_file[255] = {'\0'};

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        if (!ConvertStringToUI64(optarg, &k)) return 1;
        break;
      case 1:
        if (!ConvertStringToUI64(optarg, &mod)) return 1;
        if (mod == 0) {
          fprintf(stderr, "--mod must be > 0\n");
          return 1;
        }
        break;
      case 2:
        strncpy(servers_file, optarg, sizeof(servers_file) - 1);
        servers_file[sizeof(servers_file) - 1] = '\0';
        break;
      }
    } break;
    case '?': break;
    }
  }

  if (k == (uint64_t)-1 || mod == (uint64_t)-1 || !strlen(servers_file)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
    return 1;
  }

  unsigned int servers_num = 0;
  struct Server *servers = ReadServers(servers_file, &servers_num);
  if (servers == NULL || servers_num == 0) {
    fprintf(stderr, "No servers found in %s\n", servers_file);
    free(servers);
    return 1;
  }

  struct ClientTask *tasks = calloc(servers_num, sizeof(struct ClientTask));
  if (!tasks) {
    free(servers);
    return 1;
  }

  uint64_t chunk = (k + servers_num - 1) / servers_num;
  for (unsigned int i = 0; i < servers_num; i++) {
    tasks[i].server = servers[i];
    tasks[i].mod = mod;
    uint64_t begin = (uint64_t)i * chunk + 1;
    uint64_t end = (uint64_t)(i + 1) * chunk;
    if (end > k) end = k;
    if (begin > k) { begin = 1; end = 0; }
    tasks[i].begin = begin;
    tasks[i].end = end;
  }

  pthread_t *threads = malloc(servers_num * sizeof(pthread_t));
  for (unsigned int i = 0; i < servers_num; i++) {
    pthread_create(&threads[i], NULL, ServerWorker, &tasks[i]);
  }

  for (unsigned int i = 0; i < servers_num; i++) {
    pthread_join(threads[i], NULL);
  }

  uint64_t answer = 1 % mod;
  unsigned int failed = 0;
  for (unsigned int i = 0; i < servers_num; i++) {
    if (tasks[i].ok) {
      answer = MultModulo(answer, tasks[i].result, mod);
    } else {
      failed++;
    }
  }

  if (failed == 0) {
    printf("answer: %llu\n", (unsigned long long)answer);
  } else {
    fprintf(stderr, "Computation incomplete: %u server(s) failed\n", failed);
  }

  free(threads);
  free(tasks);
  free(servers);

#ifdef _WIN32
  WSACleanup();
#endif
  return failed == 0 ? 0 : 2;
}