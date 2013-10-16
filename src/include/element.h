/*
 */
#ifndef _ELEMENT_H_
#define _ELEMENT_H_

#include <stdbool.h>

#include <link.h>
#include <stddef.h>	// size_t
#include <assert.h>

#define MAX_PARAMS	4

typedef struct sElementDef	tElementDef;
typedef struct sElement	tElement;

struct sElementDef
{
	tElementDef	*Next;
	const char	*Name;
	 int	MinInput, MaxInput;	// -1: None
	 int	SettleTime;	// set to 0 to disable settle disable
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

	struct sBlock	*Block;

	 int	NParams;
	 int	Params[MAX_PARAMS];

	 int	NInputs;
	tLink	**Inputs;
	tLinkValue	**InVals;
	
	 int	NOutputs;
	tLink	**Outputs;
	tLinkValue	**OutVals;

	bool	*SettledOutput;
};


extern tElement	*EleHelp_CreateElement(size_t NInputs, size_t NOutputs, size_t ExtraData);

static inline bool GetEleLink(tElement *Ele, int InputNum)
{
	assert(InputNum < Ele->NInputs);
	//return !!Ele->Inputs[InputNum]->Value->Value;
	return !!Ele->InVals[InputNum]->Value;
}

static inline void SetEleLink(tElement *Ele, int OutputNum, bool Value)
{
	assert(OutputNum < Ele->NOutputs);
	if( Value ) {
		//Ele->Outputs[OutputNum]->Value->NDrivers ++;
		Ele->OutVals[OutputNum]->NDrivers ++;
	}
	Ele->SettledOutput[OutputNum] = Value;
}

#endif
