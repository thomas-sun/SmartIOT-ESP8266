#include "ets_sys.h"

#include <stdarg.h>
#include <string.h>


#define MAXLN 256
#define	_U	01
#define	_L	02
#define	_N	04
#define	_S	010
#define _P	020
#define _C	040
#define _X	0100
#define	_B	0200
#define	isspace(c)	((_ctype_+1)[(unsigned)(c)]&_S)
#define ISSPACE " \t\n\r\f\v"

typedef	unsigned long	u_long;
typedef unsigned int u_int32_t;


const char _ctype_[1 + 256] = {
	0,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
   _S|(char)_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N,	_N,	_N,	_N,	_N,	_N,	_N,	_N,
	_N,	_N,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C
};

static char *
ICACHE_FLASH_ATTR _getbase(char *p, int *basep)
{
	if (p[0] == '0') {
		switch (p[1]) {
		case 'x':
			*basep = 16;
			break;
		case 't': case 'n':
			*basep = 10;
			break;
		case 'o':
			*basep = 8;
			break;
		default:
			*basep = 10;
			return (p);
		}
		return (p + 2);
	}
	*basep = 10;
	return (p);
}


/*
 *  _atob(vp,p,base)
 */
static int
#ifdef HAVE_QUAD
ICACHE_FLASH_ATTR _atob (u_quad_t *vp, char *p, int base)
{
	u_quad_t value, v1, v2;
#else
ICACHE_FLASH_ATTR _atob (unsigned long *vp, char *p, int base)
{
	u_long value, v1, v2;
#endif
	char *q, tmp[20];
	int digit;

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		base = 16;
		p += 2;
	}

	if (base == 16 && (q = strchr (p, '.')) != 0) {
		if (q - p > sizeof(tmp) - 1)
			return (0);

		strncpy (tmp, p, q - p);
		tmp[q - p] = '\0';
		if (!_atob (&v1, tmp, 16))
			return (0);

		q++;
		if (strchr (q, '.'))
			return (0);

		if (!_atob (&v2, q, 16))
			return (0);
		*vp = (v1 << 16) + v2;
		return (1);
	}

	value = *vp = 0;
	for (; *p; p++) {
		if (*p >= '0' && *p <= '9')
			digit = *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			digit = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			digit = *p - 'A' + 10;
		else
			return (0);

		if (digit >= base)
			return (0);
		value *= base;
		value += digit;
	}
	*vp = value;
	return (1);
}

/*
 *  atob(vp,p,base) 
 *      converts p to binary result in vp, rtn 1 on success
 */
int
ICACHE_FLASH_ATTR atob(u_int32_t *vp, char *p, int base)
{
#ifdef HAVE_QUAD
	u_quad_t v;
#else
	u_long  v;
#endif

	if (base == 0)
		p = _getbase (p, &base);
	if (_atob (&v, p, base)) {
		*vp = v;
		return (1);
	}
	return (0);
}



/*
 *  vsscanf(buf,fmt,ap)
 */
static int
ICACHE_FLASH_ATTR vsscanf (const char *buf, const char *s, va_list ap)
{
    int             count, noassign, width, base, lflag;
    const char     *tc;
    char           *t, tmp[MAXLN];

    count = noassign = width = lflag = 0;
    while (*s && *buf) {
	while (isspace (*s))
	    s++;
	if (*s == '%') {
	    s++;
	    for (; *s; s++) {
		if (strchr ("dibouxcsefg%", *s))
		    break;
		if (*s == '*')
		    noassign = 1;
		else if (*s == 'l' || *s == 'L')
		    lflag = 1;
		else if (*s >= '1' && *s <= '9') {
		    for (tc = s; isdigit (*s); s++);
		    strncpy (tmp, tc, s - tc);
		    tmp[s - tc] = '\0';
		    atob (&width, tmp, 10);
		    s--;
		}
	    }
	    if (*s == 's') {
		while (isspace (*buf))
		    buf++;
		if (!width)
		    width = strcspn (buf, ISSPACE);
		if (!noassign) {
		    strncpy (t = va_arg (ap, char *), buf, width);
		    t[width] = '\0';
		}
		buf += width;
	    } else if (*s == 'c') {
		if (!width)
		    width = 1;
		if (!noassign) {
		    strncpy (t = va_arg (ap, char *), buf, width);
		    t[width] = '\0';
		}
		buf += width;
	    } else if (strchr ("dobxu", *s)) {
		while (isspace (*buf))
		    buf++;
		if (*s == 'd' || *s == 'u')
		    base = 10;
		else if (*s == 'x')
		    base = 16;
		else if (*s == 'o')
		    base = 8;
		else if (*s == 'b')
		    base = 2;
		if (!width) {
		    if (isspace (*(s + 1)) || *(s + 1) == 0)
			width = strcspn (buf, ISSPACE);
		    else
			width = strchr (buf, *(s + 1)) - buf;
		}
		strncpy (tmp, buf, width);
		tmp[width] = '\0';
		buf += width;
		if (!noassign)
		    atob (va_arg (ap, u_int32_t *), tmp, base);
	    }
	    if (!noassign)
		count++;
	    width = noassign = lflag = 0;
	    s++;
	} else {
	    while (isspace (*buf))
		buf++;
	    if (*s != *buf)
		break;
	    else
		s++, buf++;
	}
    }
    return (count);
}


/*
 *  sscanf(buf,fmt,va_alist)
 */
int 
ICACHE_FLASH_ATTR sscanf (const char *buf, const char *fmt, ...)
{

	    int             count;
    va_list ap;
    
    va_start (ap, fmt);
    count = vsscanf (buf, fmt, ap);
    va_end (ap);
    return (count);

}