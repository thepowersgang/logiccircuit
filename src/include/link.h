/*
 */
#ifndef _LINK_H_
#define _LINK_H_

typedef struct sLink	tLink;

struct sLink
{
	tLink	*Next;
	 int	Value;
	 int	NDrivers;	//!< Number of elements raising the line
	tLink	*Link;	//!< Allows aliasing (1 deep only)
	tLink	*Backlink;	//!< Used by #defunit when importing
	char	Name[];
};

#endif
