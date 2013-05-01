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
#include <string.h>

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
	// Mark #testassert items
	for(tAssertion *a = Root->Assertions; a; a = a->Next)
	{
		listiter(a->Condition, nRead);
		listiter(a->Expected, nRead);
		listiter(a->Values, nRead);
	}
//	((struct sUsage*)Root->CompletionCondition->Value->Info)->++;
//	listiter(Root->CompletionCond, nRead);
	
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
	
	for(int i = 0; i < Root->Inputs.NItems; i ++) {
		if( ((struct sUsage*)Root->Inputs.Items[i]->Value->Info)->nSet != 1 )
			fprintf(stderr, "notice: Input link %s:'%s' set\n", Root->Name, Root->Inputs.Items[i]->Name);
	}
	
	free(usage);
}
	
void Sim_DuplicateCheck(tExecUnit *Root)
{	
	// TODO: Move this to another function, as it's expensive as hell
	// Check for unessesarily duplicated elements (two elements that do the same thing on the same inputs)
	 int	nEles = 0;
	for(tElement *ele = Root->Elements; ele; ele = ele->Next)
		nEles++;
	char *visited_eles = calloc(1, nEles);
	int idx = 0;
	for(tElement *ele = Root->Elements; ele; ele = ele->Next, idx ++)
	{
		if( visited_eles[idx] )
			continue ;

		// Only flag anon targets
		// - TODO: Flag single-write links too
		if( ele->Outputs[0]->Name[0] )
			continue ;
		
		 int	nDup = 0;
		 int	idx2 = idx+1;
		
		for( tElement *ele2 = ele->Next; ele2; ele2 = ele2->Next, idx2 ++ )
		{
			if( ele->Type != ele2->Type )
				continue ;
			if( ele->NInputs != ele2->NInputs )
				continue ;
			if( ele->NParams != ele2->NParams )
				continue;
			if( memcmp(ele->Params, ele2->Params, ele->NParams*sizeof(int)) != 0 )
				continue ;
			if( memcmp(ele->Inputs, ele2->Inputs, ele->NInputs*sizeof(tLink*)) != 0 )
				continue ;
			if( ele2->Outputs[0]->Name[0] )
				continue ;
			nDup ++;
			visited_eles[idx2] = 1;
		}

		if( nDup > 0 )
		{
			// TODO: Could silently merge elements
			fprintf(stderr, "%s: %i extra of %s ", Root->Name, nDup, ele->Type->Name);
			for( int i = 0; i < ele->NInputs; i ++ ) {
				if( ele->Inputs[i]->Name[0] )
					fprintf(stderr, "%s", ele->Inputs[i]->Name);
				else
					fprintf(stderr, "%p", ele->Inputs[i]);
				if( i < ele->NInputs-1 )
					fprintf(stderr, ", ");
			}
			fprintf(stderr, "\n");
		}
	}
	free(visited_eles);
}

void Sim_CreateMesh_IMerge(tList *Left, tList *Right)
{
	tLink	*ref_in_a[Left->NItems];
	tLink	*def_in_a[Right->NItems];
	tList	ref_in, def_in;
	
	ref_in.NItems = Left->NItems;
	ref_in.Items = ref_in_a;
	for(int i = 0; i < ref_in.NItems; i ++ ) {
		assert(Left->Items[i]->Link);
		ref_in_a[i] = Left->Items[i]->Link;
	}
	
	def_in.NItems = Right->NItems;
	def_in.Items = def_in_a;
	for(int i = 0; i < def_in.NItems; i ++ ) {
		assert(Right->Items[i]->Link);
		def_in_a[i] = Right->Items[i]->Link;
	}
	
	List_EquateLinks(&ref_in, &def_in);
}

void Sim_CreateMesh_LinkList(tList *Dst, const tList *Src)
{
	assert(Dst->NItems == Src->NItems);
	for( int i = 0; i < Src->NItems; i ++ ) {
		assert(Src->Items[i]->Link);
		Dst->Items[i] = Src->Items[i]->Link;
	}
}

void Sim_CreateMesh_ImportBlock(tExecUnit *DstUnit, const tExecUnit *SrcUnit)
{
	for( tLink *link = SrcUnit->Links; link; link = link->Next )
		link->Link = NULL;
	// Duplicate values/links
	for( tLinkValue *val = SrcUnit->Values; val; val = val->Next )
	{
		tLinkValue	*newval = calloc(sizeof(tLinkValue), 1);
		assert(newval);		

		// Add to value list
		newval->Next = DstUnit->Values;
		DstUnit->Values = newval;
	
		for( const tLink *link = val->FirstLink; link; link = link->ValueNext )
		{
			assert(!link->Link);

			tLink *newlink = malloc(sizeof(tLink) + strlen(link->Name) + 1);
			assert(newlink);
			
			((tLink*)link)->Link = newlink;
			
			strcpy(newlink->Name, link->Name);
			newlink->Value = newval;
		
			newlink->ValueNext = newval->FirstLink;
			newval->FirstLink = newlink;
		
			newlink->Next = DstUnit->Links;
			DstUnit->Links = newlink;
		}
	}
	for( const tLink *link = SrcUnit->Links; link; link = link->Next )
	{
		if( Build_IsConstValue(link->Value) )
		{
			tLink *newlink = malloc(sizeof(tLink) + strlen(link->Name) + 1);
			assert(newlink);
			
			((tLink*)link)->Link = newlink;

			strcpy(newlink->Name, link->Name);
						
			newlink->Value = link->Value;
			
			// Don't bother :)
		
			newlink->Next = DstUnit->Links;
			DstUnit->Links = newlink;

			continue ;
		}
		if( !link->Link && link->ReferenceCount )
		{
			fprintf(stderr, "%s: %p(%s) [%i] was not renamed (val %p)\n",
				SrcUnit->Name, link, link->Name, link->ReferenceCount, link->Value);
			for( tLink *l2 = link->Value->FirstLink; l2; l2 = l2->ValueNext )
				fprintf(stderr, " %p(%s)", l2, l2->Name);
			fprintf(stderr, "\n");
			assert(link->Link);
		}
	}
	
	// Duplicate elements
	for( const tElement *ele = SrcUnit->Elements; ele; ele = ele->Next )
	{
		tElement *newele = Build_DuplicateElement(ele);
		newele->Next = DstUnit->Elements;
		DstUnit->Elements = newele;
		for( int i = 0; i < newele->NInputs; i ++ )
			newele->Inputs[i] = newele->Inputs[i]->Link;
		for( int i = 0; i < newele->NOutputs; i ++ )
			newele->Outputs[i] = newele->Outputs[i]->Link;
	}
	
	// Display Items
	for(const tDisplayItem *disp = SrcUnit->DisplayItems; disp; disp = disp->Next )
	{
		size_t	size = sizeof(tDisplayItem) + (disp->Condition.NItems + disp->Values.NItems)*sizeof(tLink*)
			+ strlen(disp->Label) + 1;
		tDisplayItem *newdisp = malloc(size);
		assert(newdisp);
		memcpy(newdisp, disp, size);
		newdisp->Condition.Items = (void*)(newdisp->Label + strlen(disp->Label) + 1);
		newdisp->Values.Items = newdisp->Condition.Items + newdisp->Condition.NItems;

		newdisp->Next = DstUnit->DisplayItems;
		DstUnit->DisplayItems = newdisp;

		Sim_CreateMesh_LinkList(&newdisp->Condition, &disp->Condition);
		Sim_CreateMesh_LinkList(&newdisp->Values, &disp->Values);
	}
	
	// Assertions
	for( const tAssertion *a = SrcUnit->Assertions; a; a = a->Next )
	{
		size_t	size = sizeof(tAssertion)
			+ (a->Condition.NItems + a->Values.NItems + a->Expected.NItems) * sizeof(tLink*);
		tAssertion *newa = malloc(size);
		assert(newa);
		memcpy(newa, a, size);
		newa->Condition.Items = (void*)(newa + 1);
		newa->Values.Items = newa->Condition.Items + newa->Condition.NItems;
		newa->Expected.Items = newa->Values.Items + newa->Values.NItems;
		
		newa->Next = DstUnit->Assertions;
		DstUnit->Assertions = newa;
		
		Sim_CreateMesh_LinkList(&newa->Condition, &a->Condition);
		Sim_CreateMesh_LinkList(&newa->Values, &a->Values);
		Sim_CreateMesh_LinkList(&newa->Expected, &a->Expected);
	}
	
	// Breakpoints
	for( const tBreakpoint *bp = SrcUnit->Breakpoints; bp; bp = bp->Next )
	{
		size_t	size = sizeof(tBreakpoint) + (bp->Condition.NItems)*sizeof(tLink*) + strlen(bp->Label) + 1;
		tBreakpoint *newbp = malloc(size);
		assert(newbp);
		memcpy(newbp, bp, size);
		newbp->Condition.Items = (void*)(newbp->Label + strlen(bp->Label) + 1);

		newbp->Next = DstUnit->Breakpoints;
		DstUnit->Breakpoints = newbp;		

		Sim_CreateMesh_LinkList(&newbp->Condition, &bp->Condition);
	}
	
	// Bring in imports
	for( tExecUnitRef *ref = SrcUnit->SubUnits; ref; ref = ref->Next )
	{
		// Get sub unit definition
		tUnitTemplate	*def = ref->Def;

		// Sanity check
		assert(ref->Inputs.NItems == def->Internals.Inputs.NItems);
		assert(ref->Outputs.NItems == def->Internals.Outputs.NItems);

		// Recurse
		Sim_CreateMesh_ImportBlock(DstUnit, &def->Internals);
		
		// Perform Input/Output merge
		Sim_CreateMesh_IMerge(&ref->Inputs, &def->Internals.Inputs);
		Sim_CreateMesh_IMerge(&ref->Outputs, &def->Internals.Outputs);
	}
}

tExecUnit *Sim_CreateMesh(tTestCase *Template, tLink **CompletionCond)
{
	tExecUnit	*ret;
	tExecUnit	*unit;
	
	if( Template )
		unit = &Template->Internals;
	else
		unit = &gRootUnit;
	ret = calloc(sizeof(tExecUnit), 1);
	assert(ret);
	
	Sim_CreateMesh_ImportBlock(ret, unit);

	if( CompletionCond )
	{
		if( Template && Template->CompletionCondition )
			*CompletionCond = Template->CompletionCondition->Link;
		else
			*CompletionCond = NULL;
	}
	
	return ret;
}

void Sim_FreeMesh(tExecUnit *Unit)
{
	void	*next;
	for( tLinkValue *val = Unit->Values; val; val = next )
	{
		next = val->Next;
		free(val);
	}
	for( tLink *link = Unit->Links; link; link = next )
	{
		next = link->Next;
		free(link);
	}
	for( tElement *ele = Unit->Elements; ele; ele = next )
	{
		next = ele->Next;
		free(ele);
	}
	for( tDisplayItem *di = Unit->DisplayItems; di; di = next )
	{
		next = di->Next;
		free(di);
	}
	for( tBreakpoint *bp = Unit->Breakpoints; bp; bp = next )
	{
		next = bp->Next;
		free(bp);
	}
	for( tAssertion *a = Unit->Assertions; a; a= next )
	{
		next = a->Next;
		free(a);
	}
	free(Unit);
}

void Sim_RunStep(tExecUnit *Unit)
{
	gValue_One.Value = 1;
	gValue_Zero.Value = 0;
	for( tLinkValue *val = Unit->Values; val; val = val->Next )
	{
		assert(val != val->Next);
		val->NDrivers = 0;
	}
	
	// === Update elements ===
	// TODO: Threaded?
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		assert(ele != ele->Next);
		ele->Type->Update( ele );
	}
	
	// Set values
	gValue_One.Value = 1;
	gValue_One.NDrivers = 1;
	gValue_Zero.Value = 0;
	gValue_Zero.NDrivers = 0;
	for( tLinkValue *val = Unit->Values; val; val = val->Next )
	{
		assert(val != val->Next);
		
		val->Value = (val->NDrivers != 0);
	}
}

void Sim_ShowDisplayItems(tDisplayItem *First)
{
	 int	i;
	 int	bHasDisplayed = 0;
	for( tDisplayItem *dispItem = First; dispItem; dispItem = dispItem->Next )
	{
		assert(dispItem != dispItem->Next);
		// Check condition (if one condition line is low, the item is hidden)
		for( i = 0; i < dispItem->Condition.NItems; i ++ )
		{
//			printf("%s(%p %i)\n", dispItem->Condition.Items[i]->Name,
//				dispItem->Condition.Items[i]->Value,
//				dispItem->Condition.Items[i]->Value->Value);
			if( !GetLink(dispItem->Condition.Items[i]) ) {
				break;
			}
		}
		if( i != dispItem->Condition.NItems )	continue;

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
			//printf( "%s(%i) ", List->Items[i]->Name, List->Items[i]->Value->Value);
			printf( "%s ", List->Items[i]->Name);
	}
	else {
		printf("%s(", List->Items[0]->Name);
		for( int i = 0; i < List->NItems; i ++ )
			printf( "%i", List->Items[i]->Value->Value);
		printf(")");
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

