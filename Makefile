all:
	gcc serv.c -o serv -luuid
	gcc cli.c -o cli
