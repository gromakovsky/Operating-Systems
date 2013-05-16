#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>

const char * INPUT_NAME = "input.txt";
const char * FOR_GARBAGE = "/dev/null";
const char DELIM = '\0';
const size_t CAPACITY = 4096;

int my_read(int fd, char * buf) {
    int r;
    int len = 0;
    while ((r = read(fd, buf + len, CAPACITY - len)) > 0) {
        len += r;
    }
    return r < 0 ? r : len;
}

void write_all(int fd, char * buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        int write_res = write(fd, buf, count - written);
        if (write_res < 0) {
            _exit(EXIT_FAILURE);
        }
        written += write_res;
    }
}

void parse_command(char * command, char * cur_command, char ** cur_default_args, size_t * default_args_number, size_t * max_args_number) {
    const char DELIM = ' ';
    short flag = 0;

    int i = 0;
    while (command[i + 1]) {
        if (command[i] == DELIM) {
            command[i] = 0;
            if (strlen(command) > CAPACITY) {
                _exit(EXIT_FAILURE);
            }                                    
            if (!flag) {
                strcpy(cur_command, command);
                flag = 1;
            } 
            if (*default_args_number == *max_args_number) {
                cur_default_args[*default_args_number] = malloc(sizeof(char) * CAPACITY);
                ++(*max_args_number);
            }
            strcpy(cur_default_args[*default_args_number], command);
            ++(*default_args_number);
            command += (i + 1);
            i = 0;
        } else {
            ++i;
        }
    }

    if (strlen(command) > CAPACITY) {
        _exit(EXIT_FAILURE);
    }                                    
    if (!flag) {
        strcpy(cur_command, command);
        flag = 1;
    } 
    if (*default_args_number == CAPACITY - 1) {
        _exit(EXIT_FAILURE);
    }
    if (*default_args_number == *max_args_number) {
        cur_default_args[*default_args_number] = malloc(sizeof(char) * CAPACITY);
        ++(*max_args_number);
    }
    strcpy(cur_default_args[*default_args_number], command);
    ++(*default_args_number);
}

short run(char * cur_command, char ** cur_default_args, size_t default_args_number, char * arg, int pipefd[2]) {
    short OK = 0;

    char * tmp = cur_default_args[default_args_number];
    cur_default_args[default_args_number] = arg;
    char * tmp2 = cur_default_args[default_args_number + 1];
    cur_default_args[default_args_number + 1] = NULL;

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
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
/*        int garbage = open(FOR_GARBAGE, O_WRONLY);
        dup2(garbage, STDERR_FILENO);
        close(garbage);*/
        execvp(cur_command, cur_default_args);
    }
    return OK;
}

int main() {
    int fd = open(INPUT_NAME, O_RDONLY);
    if (fd < 0) {
        _exit(EXIT_FAILURE);
    }

    int pipefd[2];

    short cur_command_known = 0;
    char * cur_command = malloc(sizeof(char) * CAPACITY);
    size_t default_args_number = 0;
    size_t max_args_number = 0;
    char ** cur_default_args = malloc(sizeof(char *) * CAPACITY);
    char * output = malloc(sizeof(char) * CAPACITY);
    char * buf = malloc(sizeof(char) * CAPACITY);
    size_t buf_len = 0;
    short eof = 0;

    while (!eof) {
        int r = read(fd, buf + buf_len, CAPACITY - buf_len);
        if (r < 0) {
            _exit(EXIT_FAILURE);
        }
        buf_len += r;
        eof = !r;
        size_t i;        
        for (i = 0; i != buf_len; ++i) {
            if (buf[i] == DELIM) {
                if (cur_command_known) {
                    pipe(pipefd);
                    close(pipefd[1]);
                    if (run(cur_command, cur_default_args, default_args_number, buf, pipefd)) {
                        int tmp = my_read(pipefd[0], output);
                        if (tmp < 0) {
                            _exit(EXIT_FAILURE);
                        }
                        write_all(STDOUT_FILENO, output, tmp); 
                    }
                    close(pipefd[0]);
                } else {
                    parse_command(buf, cur_command, cur_default_args, &default_args_number, &max_args_number);
                    cur_command_known = 1;
                }
                if (buf[i + 1] == DELIM) {
                    cur_command_known = 0;
                    default_args_number = 0;
                    ++i;
                }
                buf_len -= (i + 1);
                memmove(buf, buf + i + 1, buf_len);
                i = -1;
            }
        } 
    }

    free(buf);
    size_t i;
    for (i = 0; i != max_args_number; ++i) {
        free(cur_default_args[i]);
    }
    free(output);
    free(cur_default_args);
    free(cur_command);
    _exit(EXIT_SUCCESS);
}
