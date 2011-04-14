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
	 int	Size;
	 int	*CurVal;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	 int	size = 0;
	
	if(NParams >= 1)	size = Param[0];
	
	if(size <= 0)	size = 1;
	
	if(NInputs != 2 + size)	return NULL;


	ret = calloc( 1, sizeof(t_element) + (NInputs+1+size)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->CurVal = (void*)&ret->_links[1+size+2+size];
	ret->Size = size;
	
	ret->Ele.NOutputs = 1+size;
	ret->Ele.NInputs = 2+size;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1+size];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[Source->NOutputs];
	ret->CurVal = (void*)&ret->_links[Source->NOutputs+Source->NInputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	 int	i;
	
	if( GetLink(Ele->Inputs[1]) )	// Reset
		memset(this->CurVal, 0, sizeof(*this->CurVal)*this->Size);
	else
	{	// Set
		for( i = 0; i < this->Size; i ++ )
			if( GetLink(Ele->Inputs[2+i]) )
				this->CurVal[i] = 1;
	}
	
	// Pulse
	if( GetLink(Ele->Inputs[0]) )	RaiseLink(Ele->Outputs[0]);
	
	// Output
	for( i = 0; i < this->Size; i ++ )
	{
		if( this->CurVal[i] )
			RaiseLink(Ele->Outputs[1+i]);
	}
}

tElementDef gElement_LATCH = {
	NULL, "LATCH",
	3, -1,
	_Create,
	_Duplicate,
	_Update
};
