//_BSD_SOURCE is required for lstat(2)
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

void * my_malloc(size_t size) {
    void * res = malloc(size);
    if (res == NULL) {
        perror("malloc");
        _exit(EXIT_FAILURE);
    }
    return res;
}

/*void * my_realloc(void * ptr, size_t size) {
    void * res = realloc(ptr, size);
    if (res == NULL) {
        perror("realloc");
        _exit(EXIT_FAILURE);
    }
    return res;
}*/

const char * DELIMITER = "/";

void scan(const char * path) {
    size_t path_len = strlen(path);

    struct stat sb;
    if (lstat(path, &sb) == -1) {
        perror("lstat");
        _exit(EXIT_FAILURE);
    }
    if (S_ISLNK(sb.st_mode)) {
        if (access(path, F_OK) == -1) {
            if (errno == ENOENT) {
                write_all(STDOUT_FILENO, path, strlen(path));
                write_all(STDOUT_FILENO, "\n", 1);
                return;
            } else {
                perror("access");
                _exit(EXIT_FAILURE);
            }
        } 
    }
    if (stat(path, &sb) == -1) {
        perror("stat");
        _exit(EXIT_FAILURE);
    }
    if (!S_ISDIR(sb.st_mode)) {
        return;
    }

    if (access(path, R_OK) == -1) {
        return;
    }
    DIR * dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");    
        _exit(EXIT_FAILURE);
    }
    struct dirent * entry = readdir(dir);
    while (entry != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            entry = readdir(dir);
            continue;
        }
        size_t new_path_len;
        char * new_path;
        if (strcmp(path + path_len - strlen(DELIMITER), DELIMITER) == 0) {
            new_path_len = path_len + strlen(entry->d_name);
            new_path = malloc(new_path_len + 1);
            strcpy(new_path, path);
            strcpy(new_path + path_len, entry->d_name);
        } else {
            new_path_len = path_len + strlen(DELIMITER) + strlen(entry->d_name);
            new_path = malloc(new_path_len + 1);
            strcpy(new_path, path);
            strcpy(new_path + path_len, DELIMITER);
            strcpy(new_path + path_len + strlen(DELIMITER), entry->d_name);
        }
        if (new_path_len > PATH_MAX) {
            write_all(STDOUT_FILENO, new_path, new_path_len);
            write_all(STDOUT_FILENO, ":\nLength is too big. Ignoring this file", strlen(":\nLength is too big. Ignoring this file"));
            
        } else {
            scan(new_path);
        }
        entry = readdir(dir);
    }
    if (errno == EBADF) {
        perror("readdir");
        _exit(EXIT_FAILURE);
    }
    closedir(dir);

}

int main(int argc, char * argv[]) {
    while (--argc) {
        scan(argv[argc]);
    }
    _exit(EXIT_SUCCESS);
}
