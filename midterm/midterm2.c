#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

const char * FIFO1 = "/tmp/midterm2_FIFO1";
const char * FIFO2 = "/tmp/midterm2_FIFO2";
const char * input_name = "input.txt";
const char delim = '\0';
const size_t capacity = 4096;

char * cur_command;
short cur_command_known = 0;
char ** cur_default_args;
int default_args_number = 0;
int max_args_number = 0;
short OK;

int pipefd[2];

void my_exit(int status) {
    remove(FIFO1);
    remove(FIFO2);
    _exit(status);
}

void parse_command(char * command) {
    const char delim = ' ';

    int i = 0;
    while (command[i]) {
        if (command[i] == delim) {
            command[i] = 0;
            if (strlen(command) > capacity) {
                my_exit(EXIT_FAILURE);
            }                                    
            if (!cur_command_known) {
                strcpy(cur_command, command);
//                printf("command: %s\n", command);
                cur_command_known = 1;
            } 
                if (default_args_number == max_args_number) {
                    cur_default_args[default_args_number] = malloc(sizeof(char) * capacity);
                    ++max_args_number;
                }
                strcpy(cur_default_args[default_args_number], command);
//                printf("args %d: %s\n", default_args_number, command);
                ++default_args_number;
            command += (i + 1);
            i = 0;
        } else {
            ++i;
        }
    }
        if (strlen(command) > capacity) {
            my_exit(EXIT_FAILURE);
        }                                    
        if (!cur_command_known) {
            strcpy(cur_command, command);
//            printf("command: %s\n", command);
            cur_command_known = 1;
        } 
        if (default_args_number == capacity - 1) {
            my_exit(EXIT_FAILURE);
        }
        if (default_args_number == max_args_number) {
            cur_default_args[default_args_number] = malloc(sizeof(char) * capacity);
            ++max_args_number;
        }
        strcpy(cur_default_args[default_args_number], command);
//            printf("args %d: %s\n", default_args_number, command);
        ++default_args_number;
 
//    printf("%s\n", cur_command);
}

void run(char * arg) {
//    write(1, "RUN ", 4);
//    write(1, cur_command, 2);
    OK = 0;

    char * tmp = cur_default_args[default_args_number];
    cur_default_args[default_args_number] = arg;
    char * tmp2 = cur_default_args[default_args_number + 1];
    cur_default_args[default_args_number + 1] = NULL;

//    printf("run command %s, arg0: %s, arg1: %s\n", cur_command, cur_default_args[0], cur_default_args[1]);

    pid_t pid = fork();
    if (pid) {
        pid_t wpid;
        int status;
        do {
            wpid = wait(&status);
            if (WIFEXITED(status) && !WEXITSTATUS(status)) {
                OK = 1;
            }
        } while (wpid != pid);
        cur_default_args[default_args_number] = tmp;
        cur_default_args[default_args_number + 1] = tmp2;

    } else {
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(cur_command, cur_default_args);
    }
}

int main(int argc, char * argv[]) {
    int fd = open(input_name, O_RDONLY);
    if (fd < 0) {
        _exit(EXIT_FAILURE);
    }
    cur_command = malloc(sizeof(char) * capacity);
    cur_default_args = malloc(sizeof(char *) * capacity);
    char * buf = malloc(sizeof(char) * capacity);
    size_t buf_len = 0;
    short eof = 0;

    if (mkfifo(FIFO1, S_IRUSR | S_IWUSR) || mkfifo(FIFO2, S_IRUSR | S_IWUSR)) {
        my_exit(EXIT_FAILURE);
    }

    signal(SIGINT, my_exit);
    signal(SIGTERM, my_exit);
    signal(SIGSEGV, my_exit);

    while (!eof) {
        int r = read(fd, buf + buf_len, capacity - buf_len);
        if (r < 0) {
            my_exit(EXIT_FAILURE);
        }
        buf_len += r;
        eof = !r;
        int i;        
        for (i = 0; i != buf_len; ++i) {
            if (buf[i] == delim) {
                if (cur_command_known) {
                    pipe(pipefd);
                    run(buf);
                } else {
                    parse_command(buf);
                }
                if (buf[i + 1] == delim) {
                    cur_command_known = 0;
                    default_args_number = 0;
                    ++i;
                }
                buf_len -= (i + 1);
                memmove(buf, buf + i + 1, buf_len);
                i = 0;
            }
        } 
    }

    free(buf);
    int i;
    for (i = 0; i != max_args_number; ++i) {
        free(cur_default_args[i]);
    }
    free(cur_default_args);
    my_exit(EXIT_SUCCESS);
}
