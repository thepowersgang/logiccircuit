/*
 */
#ifndef _LINK_H_
#define _LINK_H_

typedef struct sLink	tLink;
typedef struct sLinkValue	tLinkValue;

struct sLink
{
	tLink	*Next;
	tLink	*Link;	//!< Allows aliasing (1 deep only)
	tLink	*Backlink;	//!< Used by #defunit when importing
	
	tLinkValue	*Value;
	tLink	*ValueNext;
	
	 int	ReferenceCount;	//!< Number of references to this link
	
	char	Name[];	//!< Link name (if non-empty, should be unique)
};

struct sLinkValue
{
	 int	Value;	//!< Current value of the link
	 int	NDrivers;	//!< Number of elements raising the line
	 int	ReferenceCount;	//!< Number of references to this value
	tLink	*FirstLink;
	void	*Info;
};

static inline int GetLink(tLink *Link)
{
	return !!Link->Value->Value;
}

static inline void RaiseLink(tLink *Link)
{
	Link->Value->NDrivers ++;
}

#endif
