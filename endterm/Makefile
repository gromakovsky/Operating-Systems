all: offtp.c client.c
	gcc -g -Wall -Werror -Wextra offtp.c -o offtp
	gcc -g -Wall -Werror -Wextra client.c -o client

offtp: offtp.c
	gcc -g -Wall -Werror -Wextra offtp.c -o offtp

client: client.c
	gcc -g -Wall -Werror -Wextra client.c -o client

clean:
	rm -f offtp client
