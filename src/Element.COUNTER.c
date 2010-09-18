/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	 int	Max;
	 int	Pos;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret = calloc( 1, sizeof(t_element) + (2+Param)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->Max = Param;
	ret->Pos = 0;
	
	ret->Ele.NOutputs = Param;
	ret->Ele.NInputs = 2;
	ret->Ele.Inputs = &ret->_links[0];
	ret->Ele.Outputs = &ret->_links[2];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	Ele->Outputs[this->Pos]->NDrivers ++;
	if( Ele->Inputs[1]->Value )
		this->Pos ++;
	if( Ele->Inputs[0]->Value )
		this->Pos = 0;
	if(this->Pos == this->Max)	this->Pos = 0;
}

tElementDef gElement_COUNTER = {
	NULL, "COUNTER",
	2, 2,
	_Create,
	_Update
};
