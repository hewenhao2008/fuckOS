#include <lib.h>
#include <stdio.h>
#include <fcntl.h>
int bol = 1;
int line = 0;

char *binaryname;

void
num(int f, const char *s)
{
	long n;
	int r;
	char c;

	while ((n = read(f, &c, 1)) > 0) {
		if (bol) {
			printf("%5d ", ++line);
			bol = 0;
		}
		if ((r = write(1, &c, 1)) != 1) {
			printf("write error copying %s: %e", s, r);
			exit();
		}
		if (c == '\n')
			bol = 1;
	}
	if (n < 0) {
		printf("error reading %s: %e", s, n);
		exit();
	}
}

int
main(int argc, char **argv)
{
	int f, i;

	binaryname = "num";
	if (argc == 1)
		num(0, "<stdin>");
	else
		for (i = 1; i < argc; i++) {
			f = open(argv[i], O_RDONLY,0);
			if (f < 0) {
				printf("can't open %s: %e", argv[i], f);
				exit();			
			} else {
				num(f, argv[i]);
				close(f);
			}
		}
	exit();
	return 0;
}
