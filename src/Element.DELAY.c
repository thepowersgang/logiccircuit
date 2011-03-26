/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
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
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;
	 int	delay_time;
	
	if( NParams > 1 )	return NULL;
	if( NParams == 0 )
		delay_time = 1;
	else
		delay_time = Params[0];

	ret = calloc( 1, sizeof(t_element) + 2*NInputs*sizeof(tLink*) + NInputs*(delay_time - 1) );
	if(!ret)	return NULL;
	
	ret->Delay = delay_time - 1;
	ret->Cache = (int8_t *) &ret->_links[NInputs*2];
	if(delay_time > 1)
		ret->Cache[0] = 0;
	
	//printf("ret->Delay = %i\n", ret->Delay);
	
	ret->Ele.NOutputs = NInputs;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[NInputs];
	return &ret->Ele;
}

// DELAY{1}:
// if(GetLink()) RaiseLink()
// DELAY{2}:
// if(saved) RaiseLink()
// saved = GetLink();
// DELAY{n}:
// if(saved[(pos+1)%(n-1)])
//   RaiseLink()
// saved[pos%(n-1)] = GetLink();

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	 int	i;
	
	// Single cycle delay
	if( this->Delay == 0 )
	{
		for( i = 0; i < Ele->NInputs; i ++ )
		{
			if( GetLink(Ele->Inputs[i]) )
				RaiseLink(Ele->Outputs[i]);
		}
	}
	else
	{
		 int	readOfs = ((this->Pos + 1) % this->Delay) * Ele->NInputs;
		 int	writeOfs = this->Pos * Ele->NInputs;
		
		for( i = 0; i < Ele->NInputs; i ++ )
		{
			//printf("DELAY{%i} Cache[%i] = %i\n",
			//	this->Delay,
			//	readOfs + i, this->Cache[ readOfs + i ]);
			if( this->Cache[ readOfs + i ] )
				RaiseLink(Ele->Outputs[i]);
	
			this->Cache[ writeOfs + i ] = GetLink(Ele->Inputs[i]);
		}
		this->Pos ++;
		if(this->Pos == this->Delay)	this->Pos = 0;
	}
}

tElementDef gElement_DELAY = {
	NULL, "DELAY",
	1, -1,
	_Create,
	_Update
};
