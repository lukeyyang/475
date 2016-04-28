/**
 * Luke Yang, 2016
 */
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <strings.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/select.h>

#define kMSG_BUF_LEN 256

const static char* kUSB_PORT = "/dev/ttyUSB0";
const static int kWAIT_SECOND = 1;
static volatile int running = 1;

void 
int_handler(int sig)
{
        signal(sig, SIG_IGN);
        fprintf(stderr, "Do you really want to quit? [Y/n]");
        char c = getchar();
        if (c == 'y' || c == 'Y') {
                running = 0;
        } else {
                signal(SIGINT, int_handler);
        }
        getchar();
}

void
get_temperature()
{
        /* open port */
        int fd = -1;
        if ((fd = open(kUSB_PORT, O_RDWR | O_NONBLOCK)) < 0) {
                perror("open()");
                return;
        }
	
        /* wait for 5 secs */
        struct timeval tv;
        tv.tv_sec = kWAIT_SECOND;
        tv.tv_usec = 0;


	/* file descriptor sets */
        fd_set rfds, wfds;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(fd, &rfds);
	FD_SET(fd, &wfds);

	/* throw a byte to ATMeg first */
	int rt = -1;
	rt = select(fd + 1, NULL, &wfds, NULL, &tv);
	if (rt == -1) {
		perror("select()");
	} else if (rt == 0) {
		fprintf(stderr, 
			"select() timed out\n");
	} else {
		char c = '0';
		int n = write(fd, &c, 1);
		if (n < 0) {
			perror("write()");
		} /* else {
			fprintf(stderr, "sent %d byte to ATmeg\n", n);
		} */
	}

	/* force a wait for the buffer to build up */
	sleep(kWAIT_SECOND);
	
	rt = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (rt == -1) {
		perror("select()");
	} else if (rt == 0) {
		fprintf(stderr, 
			"no incoming data in the past 10 seconds\n");
	} else {
		/* read buffer */
		char msg[kMSG_BUF_LEN];
		bzero(msg, kMSG_BUF_LEN);
		int n = read(fd, msg, kMSG_BUF_LEN);  
		if (n < 0) {
			perror("read()");
		}
		int i = 0;
		for (i = 0; i < n; i++) {
			char x = *(msg + i);
			if ((x >= (char)0x20) && (x <= (char)0x7e)) {
				printf("%c", x);
			}
		}
		printf("\n");
	}

        close(fd);
}

void
manual_zone_control()
{
	/* get an input from user */
	fprintf(stdout,
			"Which zone would you like to turn on? [1-8]\n");
	
	int8_t zone_num = -1;
	scanf("%hhd", &zone_num);
	if (!(zone_num > 0 && zone_num < 9)) {
		fprintf(stderr,
				"Invalid input. Returning...\n");
		return;
	}

        /* open port */
        int fd = -1;
        if ((fd = open(kUSB_PORT, O_WRONLY)) < 0) {
                perror("open()");
                return;
        }

        /* wait for 5 secs */
        struct timeval tv;
        tv.tv_sec = kWAIT_SECOND;
        tv.tv_usec = 0;

	/* file descriptor sets */
        fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	
	int rt = -1;
	rt = select(fd + 1, NULL, &wfds, NULL, &tv);
	if (rt == -1) {
		perror("select()");
	} else if (rt == 0) {
		fprintf(stderr, 
			"no incoming data in the past 10 seconds\n");
	} else {
		char c = (char) (zone_num + '0');
		int n = write(fd, (char*) &c, 1);
		if (n < 0) {
			perror("write()");
		} /* else {
			fprintf(stderr, "sent %d byte to ATmeg\n", n);
		} */
	}

        close(fd);
}

int
main()
{
        /* trap for Ctrl + C */
        signal(SIGINT, int_handler);

	while (running) {
		fprintf(stdout, 
				"What would you like to do? "\
				"These are your options\n"\
				"Type 1 [Enter] for temp/moist\n"\
				"     2 [Enter] for weather report\n"\
				"     3 [Enter] for manual set\n"\
				"     4 [Enter] for autoschedule\n"\
				"     q [Enter] to quit\n");
		char c = getchar();
		switch (c) {
		case '1':
			fprintf(stdout,
					"Please wait...\n");
			get_temperature();
			break;
		case '2':
			system("weather -f lax -q --imperial | head -n 4");
			break;
		case '3':
			fprintf(stdout,
					"Entering manual control...\n");
			manual_zone_control();
			break;
		case '4':
			break;
		case 'q':
			fprintf(stdout,
					"Goodbye\n");
			running = 0;
			break;
		default:
			break;
		}
		getchar();
	}
        return 0;
}
