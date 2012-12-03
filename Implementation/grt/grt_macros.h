#ifndef GRT_MACROS_H
#define GRT_MACROS_H

#define grt_stl_map_foreach(__type1, __type2, __struct, __idx)	\
  for (std::map<__type1,__type2>::iterator __idx = __struct.begin(),	\
	 __end = __struct.end(); __idx != __end; ++__idx)

#define grt_stl_set_foreach(__type, __struct, __idx) \
  for (std::set<__type>::iterator __idx = __struct.begin(), \
         __end = __struct.end(); __idx != __end; ++__idx)

#define grt_stl_foreach(__struct, __var) \
  for(typeof(__struct.begin()) __var = __struct.begin(), \
	__end = __struct.end(); __var != __end; ++__var)

/* The following macros were taken from the GasNet test.h distribution */
#define grt_align_up(a,b) ((((a)+(b)-1)/(b))*(b))
#define grt_align_down(a,b) (((a)/(b))*(b))

#ifndef MIN
  #define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#ifndef MAX
  #define MAX(x,y) ((x)>(y)?(x):(y))
#endif

#define GASNET_Safe(fncall) do {					\
    int _retval;							\
    if ((_retval = fncall) != GASNET_OK) {				\
      fprintf(stderr, "ERROR calling: %s\n"				\
	      " at: %s:%i\n"						\
	      " error: %s (%s)\n",					\
              #fncall, __FILE__, __LINE__,				\
              gasnet_ErrorName(_retval), gasnet_ErrorDesc(_retval));	\
      fflush(stderr);							\
      gasnet_exit(_retval);						\
    }									\
  } while(0)

#define GRT_Safe(fncall) do {				  \
    int _retval = fncall;				  \
    if (_retval != 0) {					  \
fprintf(stderr, "ERROR calling: %s\n"			  \
	" at: %s:%i\n", #fncall, __FILE__, __LINE__);	  \
perror(0);						  \
fflush(stderr);						  \
gasnet_exit(_retval);					  \
}							  \
} while (0)

#define BARRIER() do {							\
    gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);		\
    GASNET_Safe(gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS));	\
  } while (0)

#define grt_barrier() BARRIER()

#define BARRIER_N(id) do {						\
    gasnet_barrier_notify(id, 0);					\
    GASNET_Safe(gasnet_barrier_wait(id, 0));				\
  } while (0)


#define MINHEAPOFFSET		grt_align_up( 128*4096, GASNET_PAGESIZE)
#define MAX_PROCESSOR_NAME	(256)
#define GASNET_HEAP_SIZE (536870912)

/* GASNet doesn't like null pointers, even when the value is unused */

#define GASNET_SAFE_NULL(type, x) (x ? (type) x : (type) 1)

/* Macros for distributing n values onto p processors */
#define grt_distrib_start(n) \
(grt_id*(n/grt_num_procs) + MIN(grt_id,n%grt_num_procs))
#define grt_distrib_range(n) \
((n/grt_num_procs) + ((grt_id < (n%grt_num_procs)) ? 1 : 0))
#define grt_distrib_proc(n,idx) \
((idx < ((n/grt_num_procs+1)*(n%grt_num_procs))) ? (idx/(n/grt_num_procs+1)) : (n%grt_num_procs + ((idx - (n/grt_num_procs+1)*(n%grt_num_procs))/(n/grt_num_procs))))

#endif


