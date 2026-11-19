#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
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

uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1 % args->mod;
  if (args->begin > args->end) return ans;
  for (uint64_t i = args->begin; i <= args->end; i++) {
    ans = MultModulo(ans, i % args->mod, args->mod);
    if (i == UINT64_MAX) break;
  }
  return ans;
}

void *ThreadFactorial(void *args) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  uint64_t *res = malloc(sizeof(uint64_t));
  if (!res) return NULL;
  *res = Factorial(fargs);
  return (void *)res;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  int tnum = -1;
  int port = -1;

  while (true) {
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0: port = atoi(optarg); break;
      case 1: tnum = atoi(optarg); break;
      }
    } break;
    }
  }

  if (port == -1 || tnum <= 0) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

#ifdef _WIN32
  SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == INVALID_SOCKET) {
#else
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
#endif
    fprintf(stderr, "Can not create server socket!\n");
    return 1;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_val, sizeof(opt_val));

  if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Can not bind to socket!\n");
    return 1;
  }

  if (listen(server_fd, 128) < 0) {
    fprintf(stderr, "Could not listen on socket\n");
    return 1;
  }

  printf("Server listening at %d\n", port);
  fflush(stdout);

  while (true) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
#ifdef _WIN32
    SOCKET client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);
    if (client_fd == INVALID_SOCKET) {
#else
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);
    if (client_fd < 0) {
#endif
      continue;
    }

    while (true) {
      unsigned int buffer_size = sizeof(uint64_t) * 3;
      char from_client[sizeof(uint64_t) * 3];
      size_t got = 0;

      while (got < buffer_size) {
        int n = recv(client_fd, from_client + got, buffer_size - got, 0);
        if (n <= 0) { got = 0; break; }
        got += n;
      }

      if (got == 0) break;

      uint64_t begin = 0, end = 0, mod = 0;
      memcpy(&begin, from_client, sizeof(uint64_t));
      memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
      memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

      pthread_t threads[tnum];
      struct FactorialArgs args[tnum];

      uint64_t total_range = (begin <= end) ? (end - begin + 1) : 0;
      uint64_t chunk = (total_range + tnum - 1) / tnum;
      if (chunk == 0) chunk = 1;

      for (int i = 0; i < tnum; i++) {
        if (total_range == 0) {
          args[i].begin = 1; args[i].end = 0;
        } else {
          args[i].begin = begin + (uint64_t)i * chunk;
          uint64_t e = begin + (uint64_t)(i + 1) * chunk - 1;
          if (args[i].begin > end) { args[i].begin = 1; args[i].end = 0; }
          else if (e > end) args[i].end = end;
          else args[i].end = e;
        }
        args[i].mod = mod;
        pthread_create(&threads[i], NULL, ThreadFactorial, (void *)&args[i]);
      }

      uint64_t total = 1 % mod;
      for (int i = 0; i < tnum; i++) {
        uint64_t *result = NULL;
        pthread_join(threads[i], (void **)&result);
        if (result) {
          total = MultModulo(total, *result, mod);
          free(result);
        }
      }

      send(client_fd, (const char*)&total, sizeof(total), 0);
    }

#ifdef _WIN32
    closesocket(client_fd);
#else
    close(client_fd);
#endif
  }

#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}
