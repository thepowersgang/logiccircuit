/*
 */
#ifndef _LINK_H_
#define _LINK_H_

typedef struct sLink	tLink;

struct sLink
{
	tLink	*Next;
	 int	Value;	//!< Current value of the link
	 int	NDrivers;	//!< Number of elements raising the line
	tLink	*Link;	//!< Allows aliasing (1 deep only)
	tLink	*Backlink;	//!< Used by #defunit when importing
	
	 int	ReferenceCount;	//!< Number of references to this link
	 int	bDisplay;
	char	Name[];	//!< Link name (if non-empty, should be unique)
};

#endif
