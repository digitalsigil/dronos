#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>


#define FAST_TIMEOUT 3
#define BLUART_DEV "/dev/ttyPS1"
#define COPTER_MAC "001EC01B173B"
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

int debug = 1;

int
openBluART(char *dev)
{
	int	fd;
	struct	termios ti;

	if ((fd = open(dev, O_RDWR, 0)) == -1)
		return -1;

	fcntl(fd, F_SETFL, FNDELAY);

	if (tcgetattr(fd, &ti) == -1)
		return -1;


	ti.c_cflag |= CRTSCTS;

	cfsetispeed(&ti, B115200);
	cfsetospeed(&ti, B115200);
	cfmakeraw(&ti);

	if (tcsetattr(fd, TCSANOW, &ti) != 0)
		return -1;

	return fd;
}

int
readStrBluART(int fd, char *s, time_t timeout)
{
	char	b;
	char	*m;
	time_t	t0;
	int	rv;
	
	t0 = time(NULL);

	if (debug) printf("readStrBluART: \"");
	m = s;
	while (*m != '\0') {
		while (read(fd, &b, sizeof (b)) <= 0);
		//if (debug) printf(isgraph(b) ? "%c" : "\\%03o", b);
		if (debug) printf("\nreadStrBluART: \"%c\" %s", b, m);

		if (*m++ != b)
			m = s;

		if (t0 - time(NULL) > timeout) {
			if (debug) printf("\" ... timout\n");
			return -1;
		}
	}

	if (debug) printf("\" ... match\n");
	return 0;
}

void
writeStrBluART(int fd, char *s)
{
	int	n;

	if (debug) printf("writeStrBluART: \"%s\"\n", s);

	n = strlen(s);
	while (n > 0)	n -= write(fd, s, n);
}

int
scanDevBluART(int fd, char *d, int scantime)
{
	int found = 0;

	writeStrBluART(fd, "F\n");
	if (readStrBluART(fd, "AOK", FAST_TIMEOUT) != 0)
		return -1;

	if (readStrBluART(fd, d, scantime) == 0)
		found = 1;

	writeStrBluART(fd, "X\n");
	if (readStrBluART(fd, "AOK", FAST_TIMEOUT) != 0)
		return -1;

	return found ? 0 : -1;
}

int
connectDevBluART(int fd, char *d, int timeout)
{
	writeStrBluART(fd, "E,0,");
	writeStrBluART(fd, d);
	writeStrBluART(fd, "\n");
	if (readStrBluART(fd, "AOK", timeout) != 0)
		return -1;
	readStrBluART(fd, "Connected", 10);
		return -1;
	return 0;
}

int
connectBluART()
{
	int	fd;

	if (debug) printf("main: start\n");
	if ((fd = openBluART(BLUART_DEV)) == -1)
		err(1, "Couldn't open" BLUART_DEV);
	if (debug) printf("main: opened dev\n");

	writeStrBluART(fd, "+\n");
	readStrBluART(fd, "Echo", FAST_TIMEOUT);

	writeStrBluART(fd, "SF,1\n");
	readStrBluART(fd, "AOK", FAST_TIMEOUT);

	writeStrBluART(fd, "SS,C0000000\n");
	readStrBluART(fd, "AOK", FAST_TIMEOUT);

	writeStrBluART(fd, "SR,92000000\n");
	readStrBluART(fd, "AOK", FAST_TIMEOUT);

	writeStrBluART(fd, "R,1\n");
	readStrBluART(fd, "CMD", 10);

	printf("bluetooth ready\n");

	scanDevBluART(fd, COPTER_MAC, 10);
	connectDevBluART(fd, COPTER_MAC, 10);

	writeStrBluART(fd, "I\n");
	readStrBluART(fd, "MLDP", FAST_TIMEOUT);

	printf("got connection\n");

	return fd;
}

unsigned int
nextTaggedTile(int fd)
{
	char c;
	int rv;

	rv = read(fd, &c, 1);
	
	return rv <= 0 ? -1 : (int) c;
}

//int main (int argc, char **argv)
//{
//	int fd;
//	int tile;
//
//	fd = connectBluART();
//
//	while (1) {
//		do {
//			tile = nextTaggedTile(fd);
//			printf("%d\n", tile);
//		} while (tile != -1);
//		sleep(2);
//	}
//}
