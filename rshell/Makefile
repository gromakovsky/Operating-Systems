all: rshell.c client.c
	gcc -g -Wall -Wextra -Werror rshell.c -o rshell -lutil
#	gcc -g -Wall -Wextra -Werror client.c -o client -lutil

rshell: rshell.c
	gcc -g -Wall -Wextra -Werror rshell.c -o rshell -lutil

client: client.c
#	gcc -g -Wall -Wextra -Werror client.c -o client -lutil

clean:
	rm -f rshell client
