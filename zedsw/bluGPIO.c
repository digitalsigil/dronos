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


struct gpio {
	char	*root;
};


int debug = 1;


struct	gpio *initGPIO(char *);
void	freeGPIO(struct gpio *);
void	setupGPIOpin(int, int);
void	writeGPIOpin(int, int);


struct gpio *
initGPIOpin(char *root)
{
	struct	gpio *rv;
	int	n;
	
	if ((rv = malloc(sizeof (struct gpio))) == NULL)
		goto error;
	
	n = strlen(root) + 1;
	if ((rv->root = malloc(n * sizeof (char))) == NULL)
		goto error;
	strncpy(rv->root, root, n);
	
	return rv;

error:
	if (rv == NULL)
		return NULL;

	if (rv->root != NULL)
		free(rv->root);
	free(rv);
	return NULL;
}

void
freeGPIO(struct gpio * g)
{
	free(g->root);
	free(g);
}

void
setupGPIOpin(int pin, int dir)
{
	
}

int
openBluART(char *dev)
{
	int	fd;
	struct	termios ti;

	if ((fd = open(dev, O_RDWR, 0)) == -1)
		return -1;

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
	
	t0 = time(NULL);

	if (debug) printf("readStrBluART: \"");
	m = s;
	while (*m != '\0') {
		read(fd, &b, sizeof (b));
		//if (debug) printf(isgraph(b) ? "%c" : "\\%03o", b);
		printf("\nreadStrBluART: %s", m);

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
	return 0;
}

int
main(int argc, char **argv)
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

	scanDevBluART(fd, COPTER_MAC, 10);
	connectDevBluART(fd, COPTER_MAC, 10);

	writeStrBluART(fd, "I\n");
	readStrBluART(fd, "MLDP", FAST_TIMEOUT);

	return 0;
}
