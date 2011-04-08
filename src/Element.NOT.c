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
	t_element *ret = calloc( 1, sizeof(t_element) + (2*NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	ret->Ele.NOutputs = NInputs;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[NInputs];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[Source->NInputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	i;
	//printf("NOT %i(%s) = ",
	//	GetLink(Ele->Inputs[0]), Ele->Inputs[0]->Name);
	for( i = 0; i < Ele->NInputs; i ++ )
	{
		if( !GetLink(Ele->Inputs[i]) )
			RaiseLink(Ele->Outputs[i]);
	}
	//printf("%i\n", Ele->Outputs[0]->Value->NDrivers);
}

tElementDef gElement_NOT = {
	NULL, "NOT",
	1, -1,
	_Create,
	_Duplicate,
	_Update
};
