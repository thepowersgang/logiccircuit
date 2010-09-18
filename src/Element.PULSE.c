/*
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
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	if( Param < 1 )
		Param = 1;
	ret = calloc( 1, sizeof(t_element) + 2*sizeof(tLink*) + Param - 1 );
	if(!ret)	return NULL;
	
	ret->CurVal = 0;
	
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = 1;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	
	if( !this->CurVal ) {
		if( Ele->Inputs[0]->Value ) {
			this->CurVal = 1;
			Ele->Outputs[0]->NDrivers ++;
		}
	}
	else {
		if( !Ele->Inputs[0]->Value )
			this->CurVal = 0;
	}
}

tElementDef gElement_PULSE = {
	NULL, "PULSE",
	1, 1,
	_Create,
	_Update
};
