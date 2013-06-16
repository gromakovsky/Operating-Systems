all: watchthis.c brokensymlinks.c
	gcc -g -Wall -Wextra -Werror watchthis.c -o watchthis_c
	gcc -g -Wall -Wextra -Werror -D_BSD_SOURCE brokensymlinks.c -o brokensymlinks_c

watchthis: watchthis.c
	gcc -g -Wall -Wextra -Werror watchthis.c -o watchthis_c

brokensymlinks: brokensymlinks.c
	gcc -g -Wall -Wextra -Werror -D_BSD_SOURCE brokensymlinks.c -o brokensymlinks_c

clean:
	rm -f watchthis_c brokensymlinks_c
