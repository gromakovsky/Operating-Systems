#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

char * cmd;
int _argc;
char ** _argv;
int find(char delim, const char * buf, size_t from, size_t len) {
    size_t i;
    for (i = from; i != from + len; ++i) {
        if (buf[i] == delim) {
            return i;
        }        
    }
    return -1;
}

void run_cmd(char * buf, size_t from, size_t len) {
    char * last_arg = malloc(len + 1);
    memcpy(last_arg, buf + from, sizeof(char) * len);
    last_arg[len] = 0;
    _argv[_argc + 1] = last_arg;
    _argv[_argc + 2] = NULL;
    pid_t pid = fork();
    if (pid) {
        pid_t wpid;
        int status;
        do {
            wpid = wait(&status);
            if (WIFEXITED(status) && !WEXITSTATUS(status)) {
                write(1, last_arg, len);
                write(1, "\n", 1);
            }
        } while (wpid != pid);
        free(last_arg);
    } else {
        execvp(cmd, _argv);
        _exit(0);
    }
}

int main(int argc, char * argv[]) {
    int opt;
    char delim = '\n';
    int buffer_size = 4096;
    int index;
    int flag = 0;
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
            break;
        default:
            flag = 1;
            break;
        }
        if (flag) break;
    }

    cmd = malloc(1000);
    int i = 1;
    for (index = 1; index != argc; ++index) {
        if ((argv[index][0] != '-') && (argv[index][0] > '9' || argv[index][0] < '0')) {
            strcpy(cmd, argv[index]);
            break; 
        }
    }
    _argc = argc - index - 1;
    _argv = malloc(sizeof(char *) * (_argc + 3));
    bcopy(argv + index + 1, _argv + 1, _argc * sizeof(char *));
    _argv[0] = cmd;
    ++buffer_size;


    char * buffer = malloc(buffer_size + 1);
    int r;
    int eof = 0;
    int len = 0;
    int from;
    int delim_pos;
    while (!eof) {
        if (len >= buffer_size) {
            exit(EXIT_FAILURE);
        }
        r = read(0, buffer + len, buffer_size - len);
        eof = !r;
        from = len;
        len += r;
        delim_pos = find(delim, buffer, from, len - from);
        while (delim_pos >= 0) {
            run_cmd(buffer, 0, delim_pos);
            memmove(buffer, buffer + delim_pos + 1, sizeof(char) * (len - (delim_pos + 1)));
            from = 0;
            len -= delim_pos + 1;
            delim_pos = find(delim, buffer, from, len - from);
        }
    }
    if (len > 0) {
        if (len + 1 >= buffer_size) {
            exit(EXIT_FAILURE);
        }
        buffer[len + 1] = delim;
        run_cmd(buffer, 0, len + 1);
    }    


    free(cmd);
    free(buffer);
    free(_argv);
    return 0;
}
