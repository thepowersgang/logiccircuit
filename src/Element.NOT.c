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
	t_element *ret = calloc( 1, sizeof(t_element) + (2)*sizeof(tLink*) );
	if(!ret)	return NULL;
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = 1;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	//printf("NOT %i(%s) = ",
	//	GetLink(Ele->Inputs[0]), Ele->Inputs[0]->Name);
	if( !GetLink(Ele->Inputs[0]) )
		RaiseLink(Ele->Outputs[0]);
	//printf("%i\n", Ele->Outputs[0]->Value->NDrivers);
}

tElementDef gElement_NOT = {
	NULL, "NOT",
	1, 1,
	_Create,
	_Update
};
