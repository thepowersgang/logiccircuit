/*
 * CONST{busSize,value}
 * 
 * - Sets a line to an integer constant
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	uint32_t	Value;
	
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	 int	busSize;
	
	if(NParams != 2)	return NULL;
	
	if(Params[0] < 1 || Params[0] > 32)
		return NULL;
	
	busSize = Params[0];
	
	t_element *ret = calloc( 1, sizeof(t_element) + (busSize+1)*sizeof(tLink*) );
	if(!ret)	return NULL;
	ret->Value = Params[1];
	ret->Ele.NInputs = 0;
	ret->Ele.NOutputs = busSize;
	ret->Ele.Inputs = &ret->_links[0];
	ret->Ele.Outputs = &ret->_links[1];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[Source->NOutputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	i;
	t_element	*ele = (void*)Ele;
	
	if( !GetLink(Ele->Inputs[0]) )	return ;
	
	for( i = 0; i < Ele->NOutputs; i ++ )
	{
		if( ele->Value & (1 << i) )
			RaiseLink(Ele->Outputs[i]);
	}
}

tElementDef gElement_CONST = {
	NULL, "CONST",
	1, 1,
	_Create,
	_Duplicate,
	_Update
};
