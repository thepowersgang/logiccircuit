/*
 * HOLD element
 * - Holds the output high for ARG ticks after the input goes low
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	 int	Time;
	 int	*Rem;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	if( Param < 1 )
		Param = 1;
	ret = calloc( 1, sizeof(t_element) + 2*NInputs*sizeof(tLink*) + Param*sizeof(int) );
	if(!ret)	return NULL;
	
	ret->Time = Param;
	
	ret->Ele.NOutputs = NInputs;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[NInputs];
	ret->Rem = (int*)&ret->_links[NInputs*2];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	 int	i;
	
	for( i = 0; i < Ele->NInputs; i ++ )
	{
		// If the input is high, reset count
		if( GetLink(Ele->Inputs[i]) ) {
			this->Rem[i] = this->Time;
		}
		
		// If the counter is non-zero
		if(this->Rem[i]) {
			this->Rem[i] --;
			RaiseLink(Ele->Outputs[i]);
		}
	}
}

tElementDef gElement_HOLD = {
	NULL, "HOLD",
	1, -1,	// Min of 1, no max
	_Create,
	_Update
};
