#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>

int find_delim(const char delim, const char * buf, size_t from, size_t len) {
    size_t i;
    for (i = from; i != from + len; ++i) {
        if (buf[i] == delim) {
            return i;
        }        
    }
    return -1;
}

void * my_malloc(size_t size) {
    void * res = malloc(size);
    if (res == NULL) {
        perror("malloc");
        _exit(EXIT_FAILURE);
    }
    return res;
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

void run_cmd(char * buf, size_t from, size_t len, int cmd_argc, char ** cmd_argv) {
    char * last_arg = my_malloc(len + 1);
    memcpy(last_arg, buf + from, len);
    last_arg[len] = '\0'; 
    cmd_argv[cmd_argc - 1] = last_arg;
    pid_t pid = fork();
    if (pid) {
        int status;
        pid_t wpid = wait(&status);
        if (wpid != pid) {
            _exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && !WEXITSTATUS(status)) {
            write_all(1, last_arg, len);
            write_all(1, "\n", 1);
        }
        free(last_arg);
    } else {
        execvp(cmd_argv[0], cmd_argv);
        perror("exec");
        _exit(EXIT_FAILURE);
    }
}

const size_t CAPACITY = 4096;

int main(int argc, char * argv[]) {
    char delim = '\n';
    size_t buffer_size = CAPACITY;

    int opt;
    int opt_number = 1;
    while ((opt = getopt(argc, argv, "+nzb:")) != -1) {
        switch (opt) {
        case 'n':
            delim = '\n';
            break;
        case 'z':
            delim = '\0';
            break;
        case 'b':
            buffer_size = atoi(optarg);
            ++opt_number;
            break;
        }
        ++opt_number;
    }


    if (opt_number < argc && !strcmp(argv[opt_number], "--")) {
        ++opt_number;
    }
    
    if (opt_number >= argc) {
        _exit(EXIT_FAILURE);
    }

    char ** cmd_argv = my_malloc(sizeof(char *) * (argc - opt_number + 2));
    memcpy(cmd_argv, argv + opt_number, sizeof(char *) * (argc - opt_number));
    cmd_argv[argc - opt_number + 1] = NULL;
    ++buffer_size;

    char * buffer = my_malloc(buffer_size + 1);
    int eof = 0;
    size_t len = 0;
    int from;
    int delim_pos;
    while (!eof) {
        if (len >= buffer_size) {
            _exit(EXIT_FAILURE);
        }
        int r = read(0, buffer + len, buffer_size - len);
        eof = !r;
        from = len;
        len += r;
        delim_pos = find_delim(delim, buffer, from, len - from);
        while (delim_pos >= 0) {
            run_cmd(buffer, 0, delim_pos, argc - opt_number + 1, cmd_argv);
            memmove(buffer, buffer + delim_pos + 1, len - (delim_pos + 1));
            from = 0;
            len -= delim_pos + 1;
            delim_pos = find_delim(delim, buffer, from, len - from);
        }
    }

    if (len > 0) {
        if (len + 1 >= buffer_size) {
            _exit(EXIT_FAILURE);
        }
        buffer[len + 1] = delim;
        run_cmd(buffer, 0, len + 1, argc - opt_number + 1, cmd_argv);
    }    


    free(buffer);
    free(cmd_argv);
    _exit(EXIT_SUCCESS);
}
