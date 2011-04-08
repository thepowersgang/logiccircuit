/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	 int	Period;
	 int	Pos;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;

	if( NParams != 1 )	return NULL;

	ret = calloc( 1, sizeof(t_element) + (1)*sizeof(tLink*) );
	if(!ret)	return NULL;	

	ret->Period = Params[0];
	ret->Pos = 0;
	
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = 0;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = NULL;
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	if(this->Pos == 0)	RaiseLink(Ele->Outputs[0]);
	this->Pos ++;
	if(this->Pos == this->Period)	this->Pos = 0;
}

tElementDef gElement_CLOCK = {
	NULL, "CLOCK",
	0, 0,
	_Create,
	_Duplicate,
	_Update
};
