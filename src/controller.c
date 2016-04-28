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
#include <time.h>

#define kMSG_BUF_LEN 256
#define LOG_AND_PRINT 1
#define LOG_ONLY      0
#define POLL_FIRST    1

const static char* kUSB_PORT = "/dev/ttyUSB0";
const static char* kLOG_FILE = "/home/pi/Desktop/ee459/runtime/sprinkler.log";
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

int
poll_and_read_usbport(int poll_flag, int8_t* poll_number, int disp_flag)
{
        /* open port */
        int fd = -1;
        if ((fd = open(kUSB_PORT, O_RDWR | O_NONBLOCK)) < 0) {
                perror("open()");
                return -1;
        }
        
        /* prepare for waiting for 5 secs */
        struct timeval tv;
        tv.tv_sec = kWAIT_SECOND;
        tv.tv_usec = 0;

        /* file descriptor sets */
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        if (poll_flag == POLL_FIRST) {
                FD_ZERO(&wfds);
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
                        if (!poll_number) {
                                fprintf(stderr, "invalid poll number\n");
                                close(fd);
                                return -1;
                        }
                        char c = (char) ('0' + *poll_number);
                        int n = write(fd, &c, 1);
                        if (n < 0) {
                                perror("write()");
                        } 
                }
        }

        /* force a wait for the buffer to build up */
        sleep(kWAIT_SECOND);
        
        int rt = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (rt == -1) {
                perror("select()");
        } else if (rt == 0) {
                fprintf(stderr, "no incoming data in the past 10 seconds\n");
        } else {
                /* read buffer */
                char msg[kMSG_BUF_LEN];
                bzero(msg, kMSG_BUF_LEN);
                int n = read(fd, msg, kMSG_BUF_LEN);  
                if (n < 0) {
                        perror("read()");
                }

                /* prepare timestamp */
                char dt[20];
                struct tm tm;
                time_t curr_time;
                curr_time = time(NULL);
                tm = *localtime(&curr_time);
                strftime(dt, 20, "%m/%d/%Y %H:%M:%S", &tm);

                /* first, append timestamp to log */
                FILE* logfd = fopen(kLOG_FILE, "a");
                if (!logfd) {
                        perror("fopen()");
                        close(fd);
                        return -1;
                }
                
                fprintf(logfd, "%s: ", dt);

                /* pull bytes from read buffer */
                int i = 0;
                for (i = 0; i < n; i++) {
                        char x = *(msg + i);
                        if ((x >= (char) 0x20) && (x <= (char) 0x7e)) {
                                /* if user wants to have it 
                                 * printed to screen */
                                if (disp_flag == LOG_AND_PRINT) {
                                        fprintf(stdout, "%c", x);
                                }
                                fprintf(logfd, "%c", x);
                        }
                }
                if (disp_flag == LOG_AND_PRINT) {
                        fprintf(stdout, "\n");
                }
                fprintf(logfd, "\n");
                fclose(logfd);
        }
        close(fd);
        return 0;
}

int
get_status()
{
        int8_t zone_num = 0;
        int rt = poll_and_read_usbport(POLL_FIRST, &zone_num, LOG_AND_PRINT);
        return rt;
}

int
set_zone()
{
        /* get an input from user */
        fprintf(stdout, "Which zone would you like to turn on? [1-8]\n");
        
        int8_t zone_num = -1;
        scanf("%hhd", &zone_num);
        if (!(zone_num > 0 && zone_num < 9)) {
                fprintf(stderr, "Invalid input. Returning...\n");
                return -1;
        }

        int rt = poll_and_read_usbport(POLL_FIRST, &zone_num, LOG_ONLY);
        return rt;
}

int
main()
{
        /* trap for Ctrl + C */
        signal(SIGINT, int_handler);

        while (running) {
                fprintf(stdout, "What would you like to do? "\
                                "These are your options\n"\
                                "Type 1 [Enter]: sensor and zone status\n"\
                                "     2 [Enter]: weather report\n"\
                                "     3 [Enter]: set zone\n"\
                                "     4 [Enter]: autoschedule\n"\
                                "     q [Enter]: quit\n");
                char c = getchar();
                switch (c) {
                case '1':
                        fprintf(stdout, "Please wait...\n");
                        get_status();
                        break;
                case '2':
                        system("weather -f lax -q --imperial | head -n 4");
                        break;
                case '3':
                        fprintf(stdout, "Entering manual control...\n");
                        set_zone();
                        break;
                case '4':
                        break;
                case 'q':
                        fprintf(stdout, "Goodbye\n");
                        running = 0;
                        break;
                default:
                        break;
                }
                getchar();
        }
        return 0;
}
