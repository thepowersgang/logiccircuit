/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 *
 * Element.SEQUENCER.c
 * - Sequencer Element
 *
 * A combination of a multiplexer and a counter, on a pulse it sets the
 * next output line and clears the previous.
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	 int	Position;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;

	if( NParams != 1 )	return NULL;

	ret = calloc( 1, sizeof(t_element) + (3+Params[0])*sizeof(tLink*) );
	if(!ret)	return NULL;	

	ret->Position = 0;
	
	ret->Ele.NOutputs = Params[0];
	ret->Ele.NInputs = 3;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[ret->Ele.NOutputs];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[ret->Ele.NOutputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	if( GetLink(Ele->Inputs[0]) ) 
	{
		if( GetLink(Ele->Inputs[1]) )
			this->Position = 0;
		else if( GetLink(Ele->Inputs[2]) )
		{
			this->Position ++;
			if( this->Position == Ele->NOutputs )
				this->Position = 0;
		}
		else
			;
		
		RaiseLink(Ele->Outputs[this->Position]);
	}
}

tElementDef gElement_SEQUENCER = {
	NULL, "SEQUENCER",
	3, 3,
	_Create,
	_Duplicate,
	_Update
};

