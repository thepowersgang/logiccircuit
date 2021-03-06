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
	 int	Time;
	char	Rem[];
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	duration = 1;
	
	if( NParams > 0)
		duration = Params[0];
	
	tElement *ret = EleHelp_CreateElement(NInputs, NInputs, sizeof(t_info) + duration*sizeof(char));
	if(!ret)	return NULL;
	t_info	*info = ret->Info;
	
	info->Time = duration;
	memset(info->Rem, 0, NInputs);
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	
	for( int i = 0; i < Ele->NInputs; i ++ )
	{
		// If the input is high, reset count
		if( GetEleLink(Ele, i) ) {
			this->Rem[i] = this->Time;
		}
		
		// If the counter is non-zero
		SetEleLink(Ele, i, (this->Rem[i] > 0));
		if(this->Rem[i]) {
			this->Rem[i] --;
		}
	}
}

tElementDef gElement_HOLD = {
	NULL, "HOLD",
	1, -1,	// Min of 1, no max
	0,	// Disable settle optimisation (settle time is non-deteministic)
	_Create,
	NULL,
	_Update
};
