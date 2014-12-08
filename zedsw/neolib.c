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

static void writeString(char *buf)
{
	int rv;
	int fd = open("/sys/kernel/ece453_leds/write", O_RDWR);
	if (fd == -1)	err(1, "error on " __FILE__);

	do {
		rv = write(fd, buf, strlen (buf));
		if (rv == -1)	err(1, "error on " __FILE__);
		buf += rv;
	} while (strlen (buf) > 0);

	close(fd);
}

static void waitForZero()
{
	char c = '1';
	int rv;
	int fd;

	do {
		fd = open("/sys/kernel/ece453_leds/write", O_RDWR);
		if (fd == -1)	err(1, "error on " __FILE__);

		rv = read(fd, &c, 1);
		if (rv == -1)	err(1, "error on " __FILE__);
		//printf("%d %c\n", rv, c);

		close(fd);
	} while (c != '0');
}

void writeTile(unsigned int tilenum, unsigned int color)
{
	int i;
	unsigned int data;
	char buf[32];

	for (i = 0; i < 5; i++) {
		data = ((tilenum + i) << 24) | color;
		sprintf(buf, "0x%08x 0x%08x", 0x11, data);
		writeString(buf);
		waitForZero();
	}

	sprintf(buf, "0x%08x 0x%08x", 0xF1, 0);
	writeString(buf);
	waitForZero();
}

//int main(int argc, char **argv)
//{
//	printf("running\n");
//	writeTile(0, 0x0000FF);
//
//	return 0;
//}
