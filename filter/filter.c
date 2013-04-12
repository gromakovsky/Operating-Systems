#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define big_number 100

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
    char * last_arg = malloc(len);
    memcpy(last_arg, buf + from, len);
    _argv[_argc + 1] = last_arg;
    _argv[_argc + 2] = NULL;
//    printf("cmd: %s _argv[0]: %s _argv[1]: %s _argv[2]: %s\n", cmd, _argv[0], _argv[1], _argv[2]);
    execvp(cmd, _argv);
    free(last_arg);
}

int main(int argc, char * argv[]) {
    int opt;
    int n, z;
    int buffer_size = 4096;
    int index;
    while ((opt = getopt(argc, argv, "nzb:")) != -1) {
        int flag = 0;
        switch (opt) {
        case 'n':
            n = 1;
            z = 0;
            break;
        case 'z':
            z = 1;
            n = 0;
            break;
        case 'b':
            buffer_size = atoi(optarg);
            break;
        default:
            flag = 1;
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
    _argv = malloc(_argc + 3);
    bcopy(argv + index + 1, _argv + 1, _argc * 4);
//    printf("_argv[2]: %s\n", _argv[2]);
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
        while ((delim_pos = find(n ? '\n' : 0, buffer, from, len - from)) >= 0) {
            run_cmd(buffer, 0, delim_pos);
            memmove(buffer, buffer + delim_pos + 1, len - (delim_pos + 1));
            from = 0;
            len -= delim_pos;
        }
    }
    if (len > 0) {
        if (len + 1 >= buffer_size) {
            exit(EXIT_FAILURE);
        }
        buffer[len + 1] = n ? '\n' : 0;
        run_cmd(buffer, 0, len + 1);
    }    


    free(cmd);
    free(buffer);
    free(_argv);
    return 0;
}
