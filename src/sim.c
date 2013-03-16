/*
 * LogicCircuit
 * - By John HOdge (thePowersGang)
 *
 * sim.c
 * - Simulation Code
 */
#include <common.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

// === PROTOTYPES ===
void	PrintDisplayItem(tDisplayItem *dispItem);

// === CODE ===
void GetLists(tExecUnit *Root, tLink ***links, tElement ***elements, tDisplayItem ***dispitems)
{
	assert(Root);
	
	*links = &Root->Links;
	*elements = &Root->Elements;
	if(dispitems)	*dispitems = &Root->DisplayItems;
}

void Sim_UsageCheck(tExecUnit *Root)
{
	struct sUsage {
		 int	nSet;
		 int	nRead;
	}	*usage;
	 int	nValues = 0;
	tLink	**links;
	tElement	**elements;
	
	GetLists(Root, &links, &elements, NULL);

	for( tLink *link = *links; link; link = link->Next )
		link->Link = NULL;
	
	// Count values and allocate count space
	for( tLink *link = *links; link; link = link->Next )
	{
		if(link->Link)	continue;
		link->Link = link;
		for(tLink *l2 = link->Next; l2; l2 = l2->Next) {
			if(l2->Value == link->Value)
				l2->Link = link;
		}
		nValues ++;
	}
	
	usage = calloc(nValues, sizeof(*usage));
	
	// Set the ->Link field to the usage structure
	{
		 int	i = 0;
		for( tLink *link = *links; link; link = link->Next )
		{
			if( link->Link == link ) {
				assert(i < nValues);
				link->Value->Info = (void*)&usage[i++];
				// '0' and '1' are suppressed
				if( Build_IsConstValue(link->Value) ) {
					usage[i-1].nRead = 1;
					usage[i-1].nSet= 1;
				}
			}
			else {
			}
		}
	}

	#define listiterv(count,items,field)	do{\
		for(int i=0;i<count;i++)\
			((struct sUsage*)(items)[i]->Value->Info)->field++;\
	}while(0)
	#define listiter(list,field)	listiterv(list.NItems, list.Items, field)
	// Mark inputs/outputs
	listiter(Root->Inputs, nSet);
	listiter(Root->Outputs, nRead);
	// Mark #display items
	for( tDisplayItem *disp = Root->DisplayItems; disp; disp = disp->Next )
	{
		listiter(disp->Condition, nRead);
		listiter(disp->Values, nRead);
	}
	
	// Iterate over elements
	for( tElement *ele = *elements; ele; ele = ele->Next )
	{
		listiterv(ele->NInputs, ele->Inputs, nRead);
		listiterv(ele->NOutputs, ele->Outputs, nSet);
	}
	// Iterate over sub-units
	for( tExecUnitRef *ref = Root->SubUnits; ref; ref = ref->Next )
	{
		listiter(ref->Inputs, nRead);
		listiter(ref->Outputs, nSet);
	}
	#undef listiter
	#undef listiterv
	
	// Complain for links that are not set/read
	for( tLink *link = *links; link; link = link->Next )
	{
		assert(link != link->Next);
		assert(link->Link);
		assert(link->Value);
		assert(link->Value->Info);

		if(link->Link != link)
			continue ;

		struct sUsage	*u = (void*)link->Value->Info;
		if( u->nSet && u->nRead )
		{
			// Fully used, good
		}
		else
		{
			fprintf(stderr, "[%i,%i] Link set %s:(", u->nSet, u->nRead, Root->Name);
			assert(link->Value->FirstLink);
			for( tLink *l2 = link->Value->FirstLink; l2; l2 = l2->ValueNext )
			{
				if( l2->Name[0] == '\0' )
					fprintf(stderr, "%p", l2);
				else
					fprintf(stderr, "'%s'", l2->Name);
				if( l2->ValueNext )
					fprintf(stderr, ",");
			}
			if( u->nSet ) {
				fprintf(stderr, ") is never read\n");
#if 0
				for( tElement *ele = *elements; ele; ele = ele->Next )
				{
					for( int i = 0; i < ele->NOutputs; i ++ ) {
						if( ele->Outputs[i]->Value == link->Value )
							fprintf(stderr, "- %s %s...\n",
								ele->Type->Name,
								ele->Inputs[0]->Name);
					}
				}
#endif
			}
			else if( u->nRead )
				fprintf(stderr, ") is never set\n");
			else
				fprintf(stderr, ") is never used\n");
		}
		link->Link = NULL;
	}
	
	free(usage);
}

void Sim_RunStep(tTestCase *TestCase)
{
	tLink	**links;
	tElement	**elements;
	
	if( TestCase == NULL )
		GetLists(&gRootUnit, &links, &elements, NULL);
	else
		GetLists(&TestCase->Internals, &links, &elements, NULL);

	for( tLink *link = *links; link; link = link->Next )
	{
		assert(link != link->Next);
		link->Value->NDrivers = 0;
		if( link->Name[0] == '1' )
		{
			link->Value->Value = 1;
			link->Value->NDrivers = 1;
		}
		else if( link->Name[0] == '0' )
		{
			link->Value->Value = 0;
			link->Value->NDrivers = 0;
		}
	}
	
	// === Update elements ===
	for( tElement *ele = *elements; ele; ele = ele->Next )
	{
		assert(ele != ele->Next);
		ele->Type->Update( ele );
	}
	
	// Set values
	for( tLink *link = *links; link; link = link->Next )
	{
		assert(link != link->Next);
		link->Value->Value = !!link->Value->NDrivers;
		
		// Ensure 0 and 1 stay as 0 and 1
		if( link->Name[0] == '1' )
		{
			link->Value->Value = 1;
			link->Value->NDrivers = 1;
		}
		else if( link->Name[0] == '0' )
		{
			link->Value->Value = 0;
		}
	}
	
	for( tLink *link = *links; link; link = link->Next )
	{
		assert(link != link->Next);
		link->Value->NDrivers = 0;
		if( link->Name[0] == '1' )
		{
			assert( link->Value->Value );
			link->Value->Value = 1;
			link->Value->NDrivers = 1;
		}
		else if( link->Name[0] == '0' )
		{
			assert( !link->Value->Value );
			link->Value->Value = 0;
			link->Value->NDrivers = 0;
		}
	}
}

void Sim_ShowDisplayItems(tDisplayItem *First)
{
	 int	i;
	 int	bHasDisplayed = 0;
	for( tDisplayItem *dispItem = First; dispItem; dispItem = dispItem->Next )
	{
		assert(dispItem != dispItem->Next);
		// Check condition (if one condition line is high, the item is displayed)
		for( i = 0; i < dispItem->Condition.NItems; i ++ )
		{
//			printf("%s(%p %i)\n", dispItem->Condition.Items[i]->Name,
//				dispItem->Condition.Items[i]->Value,
//				dispItem->Condition.Items[i]->Value->Value);
			if( GetLink(dispItem->Condition.Items[i]) ) {
				break;
			}
		}
		if( i == dispItem->Condition.NItems )	continue;

		if( !bHasDisplayed ) {
			printf("-----\n");
			bHasDisplayed = 1;
		}

		PrintDisplayItem(dispItem);
	}
}

int Sim_CheckBreakpoints(tExecUnit *Unit)
{
	 int	i;
	 int	breakPointFired = 0;
	for( tBreakpoint *bp = Unit->Breakpoints; bp; bp = bp->Next )
	{
		assert(bp != bp->Next);
		// Check condition (if one condition line is high, the item is displayed)
		for( i = 0; i < bp->Condition.NItems; i ++ )
		{
			if( GetLink(bp->Condition.Items[i]) ) {
				break;
			}
		}
		if( i == bp->Condition.NItems )	continue;
		
		if( ! breakPointFired )	printf("\n");
		printf("BREAKPOINT %s\n", bp->Label);
		breakPointFired = 1;
	}
	return breakPointFired;
}

void DumpList(const tList *List, int bShowNames)
{
	if( bShowNames ) {
		for( int i = 0; i < List->NItems; i ++ )
			printf( "%s(%i) ", List->Items[i]->Name, List->Items[i]->Value->Value);
	}
	else {
		for( int i = 0; i < List->NItems; i ++ )
			printf( "%i", List->Items[i]->Value->Value);
	}
}

int Sim_CheckAssertions(tAssertion *First)
{
	 int	bFailure = 0;
	 int	assertion_num = 0;	
	for( tAssertion *a = First; a; a = a->Next, assertion_num ++ )
	{
		 int	i;
	
		for( i = 0; i < a->Condition.NItems; i ++ )
		{
			if( !GetLink(a->Condition.Items[i]) )
				break ;
		}
		// If one condition failed, don't check
		if( i < a->Condition.NItems )
			continue ;
		
		// Check that Expected == Actual
		for( i = 0; i < a->Expected.NItems; i ++ )
		{
			if( GetLink(a->Expected.Items[i]) != GetLink(a->Values.Items[i]) )
				break;
		}
		// Assertion passed
		if( i == a->Expected.NItems )
			continue ;
		
		// Failed
		printf("\n - Assertion %i failed\n", assertion_num+1);
		printf("  if ");
		DumpList(&a->Condition, 1);
		printf("assert actual ");
		DumpList(&a->Values, 0);
		printf(" == expected ");
		DumpList(&a->Expected, 0);
		bFailure = 1;
	}
	return bFailure;
}

void PrintDisplayItem(tDisplayItem *DispItem)
{
	const char	*format = DispItem->Label;
	 int	lineNum = 0;
	 int	count, i, tmpCount;
	uint32_t	val;
	
	for( ; *format; format ++ )
	{
		if( *format == '%' )
		{
			format ++;
			count = 0;
			while( isdigit(*format) )
			{
				count *= 10;
				count += *format - '0';
				format ++;
			}
			
			if(count == 0)	count = 1;
			
			#define BITS_PER_BLOCK	32
			
			tmpCount = count % BITS_PER_BLOCK;
			do
			{
				if( tmpCount == 0 )
					tmpCount = BITS_PER_BLOCK;
				val = 0;
				for( i = 0; i < tmpCount && lineNum < DispItem->Values.NItems; i ++)
				{
					val *= 2;
					val += !!DispItem->Values.Items[lineNum++]->Value->Value;
				}
				//for(; i < tmpCount; i ++ )
				//	val *= 2;
				
				switch(*format)
				{
				case 'x':
					printf("%0*x", (i+3)/4, val);
					break;
				case 'i':
					printf("%i", val);
					break;
				case 'b':
				default:
					for( ; i --; )
						printf("%i", !!(val & (1 << i)));
					break;
				}
				count -= tmpCount;
				tmpCount = BITS_PER_BLOCK;
			}	while( count >= BITS_PER_BLOCK );
			#undef BITS_PER_BLOCK
		}
		else
			printf("%c", *format);
	}
	
	if(lineNum != DispItem->Values.NItems)
		printf(": ");
	for( ; lineNum < DispItem->Values.NItems; lineNum ++ )
	{
		printf("%i", DispItem->Values.Items[lineNum]->Value->Value);
	}
	printf("\n");
}

