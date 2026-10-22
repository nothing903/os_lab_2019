#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        // Дочерний процесс сразу завершается
        printf("Child process (PID: %d) is exiting...\n", getpid());
        exit(0); 
    } else if (pid > 0) {
        // Родитель НЕ вызывает wait(), поэтому ребенок станет зомби
        printf("Parent process (PID: %d) created child (PID: %d)\n", getpid(), pid);
        printf("Parent is sleeping for 30 seconds. Open another terminal and check 'ps aux | grep defunct'!\n");
        
        sleep(30); // Спим 30 секунд, чтобы успеть увидеть зомби
        
        printf("Parent woke up and is exiting.\n");
    } else {
        perror("fork failed");
        return 1;
    }
    return 0;
}
