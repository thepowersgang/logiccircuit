/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	 int	Size;
	 int	Pos;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret = calloc( 1, sizeof(t_element) + (2+Param)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->Size = Param;
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
	 int	i;
	
	// Reset
	if( GetLink(Ele->Inputs[0]) )
		this->Pos = 0;
	
	// Increment
	if( GetLink(Ele->Inputs[1]) )
		this->Pos ++;
	
	// Wrap
	if( this->Pos == (1 << this->Size) )
		this->Pos = 0;
	
	// Output
	for( i = 0; i < this->Size; i ++ ) {
		if( this->Pos & (1 << i) )
			RaiseLink(Ele->Outputs[i]);
	}
}

tElementDef gElement_COUNTER = {
	NULL, "COUNTER",
	2, 2,
	_Create,
	_Update
};
