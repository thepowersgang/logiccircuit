/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	 int	busSize = 1;
	if(NParams >= 1)
		busSize = Params[0];
	
	if(busSize < 1)	busSize = 1;
	
	if( NInputs < busSize )	return NULL;
	
	t_element *ret = calloc( 1, sizeof(t_element) + (busSize+NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	// TODO: Extend to use variable number of "tail" arguments
	ret->Ele.NOutputs = busSize;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[busSize];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	out = 1;
	 int	i, j;
	
	for( i = 0; i < Ele->NInputs - Ele->NOutputs; i++ )
	{
		out = out && GetLink(Ele->Inputs[i]);
	}
	for( j = 0; i < Ele->NInputs; i ++, j ++ )
	{
		if( out && GetLink(Ele->Inputs[i]) )
			RaiseLink(Ele->Outputs[j]);
	}
}

tElementDef gElement_AND = {
	NULL, "AND",
	1, -1,
	_Create,
	_Update
};
