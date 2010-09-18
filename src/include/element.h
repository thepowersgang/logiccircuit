/*
 */
#ifndef _ELEMENT_H_
#define _ELEMENT_H_

#include <link.h>

typedef struct sElementDef	tElementDef;
typedef struct sElement	tElement;

struct sElementDef
{
	tElementDef	*Next;
	const char	*Name;
	 int	MinInput, MaxInput;	// -1: None
	tElement	*(*Create)(int Param, int NInputs, tLink **Inputs);
	void	(*Update)(tElement *this);
};

struct sElement
{
	tElement	*Next;
	tElementDef	*Type;

	 int	Param;

	 int	NInputs;
	tLink	**Inputs;
	
	 int	NOutputs;
	tLink	**Outputs;
};

#endif
