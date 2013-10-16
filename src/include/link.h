/*
 */
#ifndef _LINK_H_
#define _LINK_H_

#include <stdbool.h>

typedef struct sLink	tLink;
typedef struct sLinkValue	tLinkValue;

struct sLink
{
	tLink	*Next;
	tLink	*Link;	//!< Allows aliasing (1 deep only)
	
	tLinkValue	*Value;
	tLink	*ValueNext;
	
	 int	ReferenceCount;	//!< Number of references to this link
	
	char	Name[];	//!< Link name (if non-empty, should be unique)
};

struct sLinkValue
{
	tLinkValue	*Next;	//!< Next link in unit
	bool	HasChanged;
	bool	Value;	//!< Current value of the link
	unsigned long	IdleTime;
	 int	NDrivers;	//!< Number of elements raising the line
	 int	ReferenceCount;	//!< Number of references to this value
	tLink	*FirstLink;
	void	*Info;
};

static inline int GetLinkVal(tLink *Link)
{
	return !!Link->Value->Value;
}

static inline void SetLinkVal(tLink *Link, bool Value)
{
	if( Value ) {
		Link->Value->NDrivers ++;
	}
}
#endif
