/*
 */
#ifndef _INTERFACE_ELEMENT_H_
#define _INTERFACE_ELEMENT_H_

#include <Class.Link.hpp>

class Element
{
public:
	 int	NInputs;
	Link	**Inputs;
	
	 int	NOutputs;
	Link	**Outputs;
	
	virtual void	Update(void) = 0;
};

#endif
