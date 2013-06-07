#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>

const char * const INPUT_NAME = "input.txt";
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

void * my_malloc(size_t size) {
    void * res = malloc(size);
    if (res == NULL) {
        perror("malloc");
        _exit(EXIT_FAILURE);
    }
    return res;
}

void my_close(int fd) {
    if (close(fd) == -1) {
        perror("close");
        _exit(EXIT_FAILURE);
    }
}

void my_dup2(int oldfd, int newfd) {
    if (dup2(oldfd, newfd) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
    }
}

void write_all(int fd, const char * buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        int write_res = write(fd, buf, count - written);
        if (write_res == -1) {
            perror("write");
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
                write_all(STDERR_FILENO, "Error, not enough capacity", strlen("Error, not enough capacity"));
                _exit(EXIT_FAILURE);
            }                                    
            if (!flag) {
                strcpy(cur_command, command);
                flag = 1;
            } 
            if (*default_args_number == *max_args_number) {
                cur_default_args[*default_args_number] = my_malloc(CAPACITY);
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
        write_all(STDERR_FILENO, "Error, not enough capacity", strlen("Error, not enough capacity"));
        _exit(EXIT_FAILURE);
    }                                    
    if (!flag) {
        strcpy(cur_command, command);
        flag = 1;
    } 
    if (*default_args_number == CAPACITY - 1) {
         write_all(STDERR_FILENO, "Error, not enough capacity", strlen("Error, not enough capacity"));
        _exit(EXIT_FAILURE);
    }
    if (*default_args_number == *max_args_number) {
        cur_default_args[*default_args_number] = my_malloc(CAPACITY);
        ++(*max_args_number);
    }
    strcpy(cur_default_args[*default_args_number], command);
    ++(*default_args_number);
}

int run(char * cur_command, char ** cur_default_args, size_t default_args_number, char * arg, int pipefd[2]) {
    int OK = 0;

    char * tmp = cur_default_args[default_args_number];
    cur_default_args[default_args_number] = arg;
    char * tmp2 = cur_default_args[default_args_number + 1];
    cur_default_args[default_args_number + 1] = NULL;

    pid_t pid = fork();
    if (pid) {
        int status;
        pid_t wpid = wait(&status);
        if (wpid != pid) {
            _exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && !WEXITSTATUS(status)) {
            OK = 1;
        }
        cur_default_args[default_args_number] = tmp;
        cur_default_args[default_args_number + 1] = tmp2;
    } else {
        my_dup2(pipefd[1], STDOUT_FILENO);
        my_close(pipefd[0]);
        my_close(pipefd[1]);
        execvp(cur_command, cur_default_args);
        perror("exec");
        _exit(EXIT_FAILURE);
    }
    return OK;
}

int main() {
    int fd = open(INPUT_NAME, O_RDONLY);
    if (fd == -1) {
        _exit(EXIT_FAILURE);
    }

    int pipefd[2];

    short cur_command_known = 0;
    char * cur_command = my_malloc(CAPACITY);
    size_t default_args_number = 0;
    size_t max_args_number = 0;
    char ** cur_default_args = my_malloc(sizeof(char *) * CAPACITY);
    char * output = my_malloc(CAPACITY);
    char * buf = my_malloc(CAPACITY);
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
                    if (pipe(pipefd) == -1) {
                        perror("pipe");
                        _exit(EXIT_FAILURE);
                    }
                    if (run(cur_command, cur_default_args, default_args_number, buf, pipefd)) {
                        my_close(pipefd[1]);
                        int tmp = my_read(pipefd[0], output);
                        if (tmp == -1) {
                            perror("read");
                            _exit(EXIT_FAILURE);
                        }
                        write_all(STDOUT_FILENO, output, tmp); 
                    }
                    my_close(pipefd[0]);
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
                i = (size_t) -1;
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
