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
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;
	 int	duration = 1;
	
	if( NParams > 0)
		duration = Params[0];
	
	ret = calloc( 1, sizeof(t_element) + 2*NInputs*sizeof(tLink*) + duration*sizeof(int) );
	if(!ret)	return NULL;
	
	ret->Time = duration;
	
	ret->Ele.NOutputs = NInputs;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[NInputs];
	ret->Rem = (int*)&ret->_links[NInputs*2];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	t_element	*ele = (void*)Source;
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*)
		+ ele->Time * sizeof(int);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[Source->NInputs];
	ret->Rem = (int*)&ret->_links[Source->NInputs*2];
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
	_Duplicate,
	_Update
};
