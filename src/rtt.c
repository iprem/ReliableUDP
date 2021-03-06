/* include rtt1 */
#include	"unprtt.h"

int		rtt_d_flag = 0;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */

//rtt is scaled up by 4
#define	RTT_RTOCALC(ptr) (((ptr)->rtt_srtt) >> 3) + (((ptr)->rtt_rttvar) >> 2)


static float
rtt_minmax(float rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void
rtt_init(struct rtt_info *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;		/* # sec since 1/1/1970 at start */

	ptr->rtt_nrexmt = 0;
	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 3000;
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
		/* first RTO */
}
/* end rtt1 */

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */

/* include rtt_ts */
uint32_t
rtt_ts(struct rtt_info *ptr)
{
	uint32_t		ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000);

	return(ts);
}

void
rtt_newpack(struct rtt_info *ptr)
{
	ptr->rtt_nrexmt = 0;
}


/*
Use to construct itimerval for setitimer

*/
struct itimerval 
rtt_start(struct rtt_info *ptr)
{

  struct itimerval timerval; 
  // You cannot specify more than one second in microseconds
  timerval.it_value.tv_sec = ptr->rtt_rto / 1000;
  // put leftover time in microsecond field
  // remember, we are working with milliseconds
  timerval.it_value.tv_usec = (ptr->rtt_rto % 1000) * 1000;

  timerval.it_interval.tv_sec = 0;
  timerval.it_interval.tv_usec = 0;

  return timerval;
}
/* end rtt_ts */

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */

/* include rtt_stop */
void
rtt_stop(struct rtt_info *ptr, uint32_t ms)
{
	int		delta;

	ptr->rtt_rtt = ms ;		/* measured RTT in ms */
	printf("MEASURED RTT: %d \n ", ms);

	/*
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 * We use floating point here for simplicity.
	 */

	delta = ptr->rtt_rtt - ( ptr->rtt_srtt >> 3);
	ptr->rtt_srtt += delta;   /* g = 1/8 */

	if (delta < 0)
		delta = -delta;				/* |delta| */

	ptr->rtt_rttvar += (delta - ptr->rtt_rttvar);	/* h = 1/4 */

	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
}
/* end rtt_stop */

/*
 * A timeout has occurred.
 * Return -1 if it's time to give up, else return 0.
 */

/* include rtt_timeout */
int
rtt_timeout(struct rtt_info *ptr)
{
	ptr->rtt_rto *= 2;		/* next RTO */

	if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return(-1);			/* time to give up for this packet */
	return(0);
}
/* end rtt_timeout */

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */

void
rtt_debug(struct rtt_info *ptr)
{
	if (rtt_d_flag == 0)
		return;

	fprintf(stderr, "rtt = %d, srtt = %d, rttvar = %d, rto = %d \n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}
