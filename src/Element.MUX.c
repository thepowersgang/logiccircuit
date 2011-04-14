/*
 * Multiplexer
 * INPUTS:
 * - Select[bits]
 * - Data[1 << bits]
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	 int	Bits;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;
	 int	bits = 1;
	 int	busSize = 1;

	if( NParams > 2 )	return NULL;
	if( NParams >= 1 )	bits = Params[0];
	if( NParams >= 2 )	busSize = Params[1];
	
	if( bits < 1 )	bits = 1;
	if( busSize < 1 )	busSize = 1;
	
	if( NInputs != bits + (1 << bits)*busSize ) {
		return NULL;
	}
	
	ret = calloc( 1, sizeof(t_element) + (NInputs + busSize)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->Bits = bits;
	ret->Ele.NInputs = NInputs;
	ret->Ele.NOutputs = busSize;
	ret->Ele.Inputs = &ret->_links[0];
	ret->Ele.Outputs = &ret->_links[NInputs];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Inputs = &ret->_links[0];
	ret->Ele.Outputs = &ret->_links[Source->NInputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*ele = (void*)Ele;
	 int	val = 0, i;
	
	// Parse inputs as a binary stream
	for( i = 0; i < ele->Bits; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			val |= 1 << i;
	}
	
	// If ENABLE, drive output
	for( i = 0; i < Ele->NOutputs; i ++)
	{
		if( GetLink(Ele->Inputs[ele->Bits + val*Ele->NOutputs + i]) )
			RaiseLink(Ele->Outputs[i]);
	}
}

tElementDef gElement_MUX = {
	NULL, "MUX",
	3, -1,
	_Create,
	_Duplicate,
	_Update
};
