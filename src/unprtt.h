#ifndef	__unp_rtt_h
#define	__unp_rtt_h

#include	"unp.h"

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, ms */
  int		rtt_srtt;	/* smoothed RTT estimator, ms */
  int		rtt_rttvar;	/* smoothed mean deviation, ms */
  int		rtt_rto;	/* current RTO to use, ms */
  int		rtt_nrexmt;	/* #times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* #sec since 1/1/1970 at start */
};



#define	RTT_RXTMIN      100	/* min retransmit timeout value, seconds */
#define	RTT_RXTMAX     5000	/* max retransmit timeout value, seconds */
#define	RTT_MAXNREXMT 	3	/* max #times to retransmit */

				/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_init(struct rtt_info *);
void	 rtt_newpack(struct rtt_info *);
struct itimerval		 rtt_start(struct rtt_info *);
void	 rtt_stop(struct rtt_info *, uint32_t);
int		 rtt_timeout(struct rtt_info *);
uint32_t rtt_ts(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set nonzero for addl info */

#endif	/* __unp_rtt_h */
