#ifndef _DEBUG_H
#define _DEBUG_H 1

#ifdef DEBUG
#define XPRINTF printf
#else
#define XPRINTF //
#endif

#endif  /* _DEBUG_H */
