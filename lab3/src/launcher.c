#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("./sequential_min_max", "sequential_min_max", "1", "10", NULL);
        perror("execl failed");
        return 1;
    } else if (pid > 0) {
        wait(NULL);
        printf("Launcher: sequential_min_max finished.\n");
    } else {
        perror("fork failed");
        return 1;
    }
    return 0;
}
