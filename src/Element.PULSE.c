/*
 * PULSE element
 * - Raises the output for only one tick when input rises
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	 int	CurVal;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Param, int NInputs, tLink **Inputs)
{
	t_element *ret;

	ret = calloc( 1, sizeof(t_element) + 2*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->CurVal = 0;
	
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = 1;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	
	// Single pulse on rising edge
	if( !this->CurVal && GetLink(Ele->Inputs[0]) ) {
		RaiseLink(Ele->Outputs[0]);
	}
	this->CurVal = !!GetLink(Ele->Inputs[0]);
}

tElementDef gElement_PULSE = {
	NULL, "PULSE",
	1, 1,
	_Create,
	_Duplicate,
	_Update
};
