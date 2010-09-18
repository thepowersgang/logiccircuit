/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret = calloc( 1, sizeof(t_element) + (1+NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	out = 0;
	 int	i;
	for( i = 0; i < Ele->NInputs; i++ )
	{
		out = out || Ele->Inputs[i]->Value;
	}
	if( out )
		Ele->Outputs[0]->NDrivers ++;
}

tElementDef gElement_OR = {
	NULL, "OR",
	1, -1,
	_Create,
	_Update
};
