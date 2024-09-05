/****************************************************************************
*
* Copyright (c) 2016 Wi-Fi Alliance
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
* SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
* USE OR PERFORMANCE OF THIS SOFTWARE.
*
*****************************************************************************/

/*
 * File: wfa_miscs.c - misc functions for agents.
 */

#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#endif

#include "wfa_portall.h"
#include "wfa_stdincs.h"
#include "wfa_debug.h"
#include "wfa_main.h"
#include "wfa_types.h"
#include "wfa_tg.h"
#include "wfa_miscs.h"

extern tgStream_t *theStreams;

tgStream_t *findStreamProfile(int id);
/*
 * printProfile(): a debugging function to display a profile info based on
 *                 a streamId
 */

void printProfile(tgProfile_t *pf)
{
    printf("profile type %i direction %i Dest ipAddr %s Dest port %i So ipAddr %s So port %i rate %i duration %i pksize %i\n", pf->profile, pf->direction, pf->dipaddr, pf->dport, pf->sipaddr, pf->sport, pf->rate, pf->duration, pf->pksize);
}

int isString(char *str)
{
    if(*str == '\0')
        return WFA_FAILURE;

    if((str[0] >= 'a' && str[0] <= 'z')
            || (str[0] > 'A' && str[0] < 'Z'))
        return WFA_SUCCESS;
    else
        return WFA_FAILURE;

}

int isNumber(char *str)
{
    if(*str == '\0')
        return WFA_FAILURE;

    if (str[0] >= '0' && str[0] <= '9')
        return WFA_SUCCESS;
    else
        return WFA_FAILURE;
}

int isIpV4Addr(char *str)
{
    int dots = 0;
    char *tmpstr = str;

    if(*str == '\0')
        return WFA_FAILURE;

    while(*tmpstr != '\0')
    {
        if(*tmpstr == '.')
        {
            dots++;
        }

        tmpstr++;
    }

    if(dots <3)
        return WFA_FAILURE;
    else
        return WFA_SUCCESS;
}

int getIPaddr(char *inf, char *IPaddr)
{
    int fd;
    struct ifreq ifr;
    char *IPaddr_buf;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* IP address attached to interface */
    strncpy(ifr.ifr_name, inf, IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    /* display result */
    IPaddr_buf = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    strcpy(IPaddr, IPaddr_buf);

    return (isIpV4Addr(IPaddr));
}
inline double wfa_timeval2double(struct timeval *tval)
{
    return ((double) tval->tv_sec + (double) tval->tv_usec*1e-6);
}

inline void wfa_double2timeval(struct timeval *tval, double dval)
{
    tval->tv_sec = (long int) dval;
    tval->tv_usec = (long int) ((dval - tval->tv_sec) * 1000000);
}

inline double wfa_ftime_diff(struct timeval *t1, struct timeval *t2)
{
    double dtime;

    dtime = wfa_timeval2double(t2) - wfa_timeval2double(t1);
    return dtime ;
}

int wfa_itime_diff(struct timeval *t1, struct timeval *t2)
{
    int dtime;
    int sec = t2->tv_sec - t1->tv_sec;
    int usec = t2->tv_usec - t1->tv_usec;

     if  (sec < 0)
     {
         //DPRINT_INFO(WFA_OUT, "wfa_itime_diff, time field ERR sec=%d \n", sec);
         return 0;
     }
     else if ( (sec == 0))
     {
         
         if ( usec >= 0)
             return usec;
         else
         {
             //DPRINT_INFO(WFA_OUT, "wfa_itime_diff, time field ERR sec=%d usec=%i\n", sec, usec);
             return 0;
         }

     }
     if(usec < 0)
     {
         sec -=1;
         usec += 1000000;
     }

    dtime = sec*1000000 + usec;
    return dtime;
}

/*
 * THe following two functions are converting Little Endian to Big Endian.
 * If your machine is already a Big Endian, you may flag it out.
 */
inline void int2BuffBigEndian(int val, char *buf)
{
    char *littleEn = (char *)&val;

    buf[0] = littleEn[3];
    buf[1] = littleEn[2];
    buf[2] = littleEn[1];
    buf[3] = littleEn[0];
}

int bigEndianBuff2Int(char *buff)
{
    int val;
    char *strval = (char *)&val;

    strval[0] = buff[3];
    strval[1] = buff[2];
    strval[2] = buff[1];
    strval[3] = buff[0];

    return val;
}

int wfa_estimate_timer_latency()
{
    struct timeval t1, t2, tp2;
    int sleep=20000; /* 20 miniseconds */
    int latency =0;

    gettimeofday(&t1, NULL);
    wUSLEEP(sleep);

    wGETTIMEOFDAY(&t2, NULL);

    tp2.tv_usec = t1.tv_usec + 20000;
    if( tp2.tv_usec >= 1000000)
    {
        tp2.tv_sec = t1.tv_sec +1;
        tp2.tv_usec -= 1000000;
    }
    else
        tp2.tv_sec = t1.tv_sec;

    return latency = (t2.tv_sec - tp2.tv_sec) * 1000000 + (t2.tv_usec - tp2.tv_usec);
}

char* replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos){
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return str;
}

int getopt(int argc, char *const argv[], const char *optstring)
{
	static int optchr = 1;
	char *cp;

	if (optchr == 1) {
		if (optind >= argc) {
			/* all arguments processed */
			return EOF;
		}

		if (argv[optind][0] != '-' || argv[optind][1] == '\0') {
			/* no option characters */
			return EOF;
		}
	}

	if (strcmp(argv[optind], "--") == 0) {
		/* no more options */
		optind++;
		return EOF;
	}

	optopt = argv[optind][optchr];
	cp = strchr(optstring, optopt);
	if (cp == NULL || optopt == ':') {
		if (argv[optind][++optchr] == '\0') {
			optchr = 1;
			optind++;
		}
		return '?';
	}

	if (cp[1] == ':') {
		/* Argument required */
		optchr = 1;
		if (argv[optind][optchr + 1]) {
			/* No space between option and argument */
			optarg = &argv[optind++][optchr + 1];
		} else if (++optind >= argc) {
			/* option requires an argument */
			return '?';
		} else {
			/* Argument in the next argv */
			optarg = argv[optind++];
		}
	} else {
		/* No argument */
		if (argv[optind][++optchr] == '\0') {
			optchr = 1;
			optind++;
		}
		optarg = NULL;
	}
	return *cp;
}
