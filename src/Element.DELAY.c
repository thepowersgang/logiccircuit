/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	 int	Delay;
	 int	Pos;
	int8_t	*Cache;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	if( Param < 1 )
		Param = 1;
	ret = calloc( 1, sizeof(t_element) + 2*sizeof(tLink*) + Param - 1 );
	if(!ret)	return NULL;
	
	ret->Delay = Param - 1;
	ret->Cache = (int8_t *) &ret->_links[2];
	if(Param > 1)
		ret->Cache[0] = 0;
	
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = 1;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	
	if( this->Delay == 0 ) {
		Ele->Outputs[0]->Value = Ele->Inputs[0]->Value;
	}
	else {
		Ele->Outputs[0]->Value = this->Cache[ (this->Pos - 1 + this->Delay) % this->Delay ];
	
		this->Cache[ this->Pos ] = Ele->Inputs[0]->Value;
		this->Pos ++;
		if(this->Pos == this->Delay)	this->Pos = 0;
	}
}

tElementDef gElement_DELAY = {
	NULL, "DELAY",
	1, 1,
	_Create,
	_Update
};
