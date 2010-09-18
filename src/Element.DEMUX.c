/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	if( Param < 1 )
		Param = 1;
	
	if( NInputs != 1 + Param ) {
		return NULL;
	}
	
	ret = calloc( 1, sizeof(t_element) + (NInputs + (1<<Param))*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->Ele.NInputs = NInputs;
	ret->Ele.Inputs = &ret->_links[0];
	ret->Ele.NOutputs = 1 << Param;
	ret->Ele.Outputs = &ret->_links[NInputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	val = 0, i;
	
	for( i = 1; i < Ele->NInputs; i ++ )
	{
		if( Ele->Inputs[i]->Value )
			val |= 1 << (i-1);
	}
	
	for( i = 0; i < Ele->NOutputs; i ++ )
		Ele->Outputs[i]->Value = 0;
	
	Ele->Outputs[1]->Value = 1;
}

tElementDef gElement_DEMUX = {
	NULL, "DEMUX",
	2, -1,
	_Create,
	_Update
};
