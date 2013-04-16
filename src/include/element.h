/*
 */
#ifndef _ELEMENT_H_
#define _ELEMENT_H_

#include <link.h>
#include <stddef.h>	// size_t

#define MAX_PARAMS	4

typedef struct sElementDef	tElementDef;
typedef struct sElement	tElement;

struct sElementDef
{
	tElementDef	*Next;
	const char	*Name;
	 int	MinInput, MaxInput;	// -1: None
	/**
	 * \param NParams	Number of parameters
	 * \param Params	List of numeric parameters
	 */
	tElement	*(*Create)(int NParams, int *Params, int NInputs);
	 int	(*Duplicate)(const tElement *this, tElement *New);
	void	(*Update)(tElement *this);
};

struct sElement
{
	tElement	*Next;
	tElementDef	*Type;
	size_t	InfoSize;
	void	*Info;

	void	*Block;

	 int	NParams;
	 int	Params[MAX_PARAMS];

	 int	NInputs;
	tLink	**Inputs;
	
	 int	NOutputs;
	tLink	**Outputs;
};


extern tElement	*EleHelp_CreateElement(size_t NInputs, size_t NOutputs, size_t ExtraData);

#endif
