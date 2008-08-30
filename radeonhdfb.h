
#define RADEONHD_DEBUGME

#ifdef RADEONHD_DEBUGME
#  define BLAH(x,...) printk(KERN_DEBUG "radeonhdfb: " x, __VA_ARGS__ )
#  define BLAH_(x) printk(KERN_DEBUG "radeonhdfb: " x)
#else
#  define BLAH(x,...)
#  define BLAH_(x)
#endif

