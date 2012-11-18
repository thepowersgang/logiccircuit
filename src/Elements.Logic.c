/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	 int	BusCount;
	
	tLink	*_links[];
}	t_element;

// === CODE ===
/**
 * \brief Create a gate
 * 
 * Parameters:
 * - 0: Bus Size (Defaults to 1)
 *  - Determines the number of trailing lines that are treated as a bus
 * - 1: Bus Count (Defaults to 1) - UNIMPLEMENTED
 *  - Determines the number of busses at the end of input
 */
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	 int	busSize = 1, busCount = 1;
	
	if(NParams > 2)	return NULL;
	
	if(NParams >= 1)	busSize = Params[0];
	if(NParams >= 2)	busCount = Params[1];
	
	if(busSize < 1)	busSize = 1;
	if(busCount < 1)	busCount = 1;
	
	if( NInputs < busSize*busCount ) {
		fprintf(stderr, "Logic _Create - Too few inputs (%i < %i*%i)\n", NInputs, busSize, busCount);
		return NULL;
	}
	
	t_element *ret = calloc( 1, sizeof(t_element) + (busSize+NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->BusCount = busCount;
	ret->Ele.NOutputs = busSize;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[busSize];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[Source->NOutputs];
	
	return &ret->Ele;
}

#define MAKE_ELE_UPDATE(__name, __init, __operation, __invert) static void _Update_##__name(tElement *Ele) { \
	 int	out = __init; \
	 int	i, j, base; \
	t_element *ele = (void*)Ele; \
	base = Ele->NInputs - Ele->NOutputs*ele->BusCount; \
	for( i = 0; i < base; i++ ) {\
		out = out __operation (!!GetLink(Ele->Inputs[i])); \
	} \
	for( j = 0; j < Ele->NOutputs; j ++ ) { \
		 int	outTmp = out; \
		for(i = 0; i < ele->BusCount; i ++) { \
			outTmp = outTmp __operation (!!GetLink(Ele->Inputs[base+i*Ele->NOutputs+j])); \
		} \
		if( outTmp == !(__invert) ) \
			RaiseLink(Ele->Outputs[j]); \
	} \
}

MAKE_ELE_UPDATE(AND, 1, &&, 0);
MAKE_ELE_UPDATE(OR,  0, ||, 0);
MAKE_ELE_UPDATE(XOR, 0, ^, 0);
MAKE_ELE_UPDATE(NAND, 1, &&, 1);
MAKE_ELE_UPDATE(NOR, 0, ||, 1);
MAKE_ELE_UPDATE(NXOR, 0, ^, 1);

#define CREATE_ELE(__name) tElementDef gElement_##__name = {\
	NULL, #__name, 1, -1, _Create, _Duplicate, _Update_##__name \
} \

CREATE_ELE(AND);
CREATE_ELE(OR);
CREATE_ELE(XOR);
CREATE_ELE(NAND);
CREATE_ELE(NOR);
CREATE_ELE(NXOR);
