/*
 * 
 */
#include <common.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#define PRINT_ELEMENTS	0

// === IMPORTS ===
extern tElementDef	*gpElementDefs;
extern tBreakpoint	*gpBreakpoints;
extern tDisplayItem	*gpDisplayItems;
extern tTestCase	*gpTests;

extern int ParseFile(const char *Filename);

// === MACRO! ===
#define ADD_ELEDEF(name)	do {\
	extern tElementDef gElement_##name;\
	gElement_##name.Next = gpElementDefs;\
	gpElementDefs = &gElement_##name;\
} while(0)

// === PROTOTYPES ===
void	RunSimulationStep(tTestCase *Root);
void	ShowDisplayItems(tDisplayItem *First);
void	ReadCommand(int MaxCmd, char *Command, int MaxArg, char *Argument);
void	SigINT_Handler(int Signum);
void	PrintDisplayItem(tDisplayItem *dispItem);
void	CompressLinks(void);
void	WriteCompiledVersion(const char *Path, int bBinary);
void	DumpList(const tList *List, int bShowNames);
void	ResolveLinks(tLink *First);
void	ResolveEleLinks(tElement *First);

// === GLOBALS ===
 int	gbRunSimulation = 1;
 int	giSimulationSteps = 0;
 int	gbStepping = 1;
 int	gbPrintLinks = 0;
 int	gbPrintStats = 0;
 int	gbCompress = 1;

 int	gbRunTests = 0;
 int	gbTest_ShowDisplay = 0;

// === CODE ===
int main(int argc, char *argv[])
{
	tLink	*link;
	tElement	*ele;
	 int	i;
	 int	timestamp;
	
	// Add element definitions
	ADD_ELEDEF( AND); ADD_ELEDEF( OR); ADD_ELEDEF( XOR);
	ADD_ELEDEF(NAND); ADD_ELEDEF(NOR); ADD_ELEDEF(NXOR);
	
	ADD_ELEDEF(NOT);
	ADD_ELEDEF(COUNTER);
	ADD_ELEDEF(MUX);
	ADD_ELEDEF(DEMUX);
	ADD_ELEDEF(LATCH);
	
	ADD_ELEDEF(CLOCK);
	
	ADD_ELEDEF(DELAY);
	ADD_ELEDEF(PULSE);
	ADD_ELEDEF(HOLD);
	ADD_ELEDEF(VALUESET);
	ADD_ELEDEF(SEQUENCER);
	
	// Load Circuit file(s)
	for( i = 1; i < argc; i ++ )
	{
		if( argv[i][0] == '-' )
		{
			if( strcmp(argv[i], "-links") == 0 )
				gbPrintLinks = 1;
			else if( strcmp(argv[i], "-stats") == 0 )
				gbPrintStats = 1;
			else if( strcmp(argv[i], "-test") == 0 )
				gbRunTests = 1;
			else if( strcmp(argv[i], "-testdbg") == 0 )
				gbTest_ShowDisplay = 1;
			else if( strcmp(argv[i], "-count") == 0 ) {
				if(i + 1 == argc)	return -1;
				giSimulationSteps = atoi(argv[++i]);
			}
			else {
				// ERROR
			}
			continue ;
		}
		
		if( ParseFile(argv[i]) ) {
			return -1;
		}
	}
	
	// Resolve links
	printf("Resolving links...\n");
	ResolveLinks(gpLinks);
	ResolveEleLinks(gpElements);
	for( tTestCase *test = gpTests; test; test = test->Next )
	{
		ResolveLinks(test->Links);
//		ResolveEleLinks(test->Elements);
	}
	
	// Print links
	// > Scan all links and find ones that share a value pointer 
	if( gbPrintLinks )
	{
		for( link = gpLinks; link; link = link->Next )
		{
			 int	linkCount;
			tLink	*link2;
			if(link->Backlink)	continue;	// Skip ones already done
			linkCount = 0;
			printf("(tValue)%p:", link->Value);
			for( link2 = gpLinks; link2; link2 = link2->Next )
			{
				if(link2->Backlink)	continue;
				if(link2->Value != link->Value)	continue;
				if( link2->Name[0] )
					printf(" '%s'", link2->Name);
				else
				#if 0
					printf(" (tLink)%p", link2);
				#else	// HACK: Relies on change in build.c
					printf(" '%s'", link2->Name+1);
				#endif
				link2->Backlink = link;	// Make backlink non-zero
				linkCount ++;
			}
			printf(" (Used %i times)", linkCount);
			printf("\n");
		}
		
		return 0;
	}

	if( gbCompress )
		CompressLinks();	

	if( gbPrintStats )
	{
		 int	totalLinks = 0;
		 int	totalNamedLinks = 0;
		 int	totalValues = 0;
		 int	totalElements = 0;
		printf("Gathering statistics...\n");
		for( link = gpLinks; link; link = link->Next )
			link->Backlink = NULL;
		for( link = gpLinks; link; link = link->Next )
		{
			tLink	*link2;

			totalLinks ++;
			
			if(link->Name[0])
			{
				totalNamedLinks ++;
			}

			if(link->Backlink)	continue;	// Skip ones already done

			for( link2 = link; link2; link2 = link2->Next )
			{
				if(link2->Backlink)	continue;
				if(link2->Value != link->Value)	continue;
				link2->Backlink = link;	// Make backlink non-zero
			}
			totalValues ++;
		}
		
		for( ele = gpElements; ele; ele = ele->Next )
			totalElements ++;

		printf("%i Links (joins)\n", totalLinks);
		printf("- %i are named\n", totalNamedLinks);
		printf("%i values (nodes)\n", totalValues);
		printf("%i elements\n", totalElements);

		return 0;
	}

	// TODO: Support saving tree to a file
	if( 0 ) {
		WriteCompiledVersion("cct.binary", 1);
		WriteCompiledVersion("cct.ascii", 0);
	}
	
	if( gbRunTests )
	{
		printf("=== Running tests ===\n");
		for( tTestCase *test = gpTests; test; test = test->Next )
		{
			 int	steps_elapsed = 0;
			 int	bFailure = 0;
			printf("Test '%s'...", test->Name);
			while( steps_elapsed != test->MaxLength && !bFailure )
			{
				RunSimulationStep(test);
				if( gbTest_ShowDisplay )
					ShowDisplayItems(test->DisplayItems);
				steps_elapsed ++;
			
				 int	assertion_num = 0;	
				for( tAssertion *a = test->Assertions; a; a = a->Next, assertion_num ++ )
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
				
				if( test->CompletionCondition && GetLink(test->CompletionCondition) )
					break;
			}
			if( bFailure == 1 ) {
				printf("\n- Test '%s' failed in %i steps\n", test->Name, steps_elapsed);
			}
			else if( steps_elapsed == test->MaxLength ) {
				printf("\n- TIMED OUT (%i cycles)\n", steps_elapsed);
			}
			else {
				printf(" PASS (%i cycles)\n", steps_elapsed);
			}
		}
		
		return 0;
	}

	signal(SIGINT, SigINT_Handler);
	
	// Go to alternte screen buffer
	if( giSimulationSteps == 0 )
	{
		printf("\x1B[?1047h");
	}
	else
	{
		gbStepping = 0;
	}
	
	// Execute
	for( timestamp = 0; gbRunSimulation; timestamp ++ )
	{
		tBreakpoint	*bp;
		 int	breakPointFired = 0;

		if( giSimulationSteps && timestamp == giSimulationSteps )
			break;	
	
		// Clear drivers
		for( link = gpLinks; link; link = link->Next ) {
			link->Value->NDrivers = 0;
			// Ensure 0 and 1 stay as 0 and 1
			if( link->Name[0] == '1' ) {
				link->Value->Value = 1;
				link->Value->NDrivers = 1;
			}
			else if( link->Name[0] == '0' )
				link->Value->Value = 0;
		}

		// === Show Display ===
		if(!giSimulationSteps)
		{
			printf("\x1B[2J");
			printf("\x1B[H");
		}
		printf("---- %6i ----\n", timestamp);
		
		ShowDisplayItems(gpDisplayItems);
	
		// Check breakpoints
		breakPointFired = 0;
		for( bp = gpBreakpoints; bp; bp = bp->Next )
		{
			ASSERT(bp != bp->Next);
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
		
		// === User Input ===
		if( giSimulationSteps == 0 && ( gbStepping || breakPointFired ) )
		{
			for( ;; )
			{
				char	commandBuffer[100];
				char	argBuffer[100];
				
				printf("> ");
				ReadCommand(
					sizeof(commandBuffer)-1, commandBuffer,
					sizeof(argBuffer)-1, argBuffer
					);
				
				// Quit
				if( strcmp(commandBuffer, "q") == 0 ) {
					gbRunSimulation = 0;
					break ;
				}
				// Run until breakpoint
				else if( strcmp(commandBuffer, "run") == 0 ) {
					gbStepping = 0;	// Disable single step
					break;
				}
				// Continue (Same as step)
				else if( strcmp(commandBuffer, "c") == 0 ) {
					gbStepping = 1;
					break;
				}
				// Step
				else if( strcmp(commandBuffer, "s") == 0 ) {
					gbStepping = 1;
					break;
				}
				// Display
				else if( strcmp(commandBuffer, "d") == 0 ) {
					// Find named link
					for( link = gpLinks; link; link = link->Next ) {
						if( strcmp(link->Name, argBuffer) == 0 ) {
							printf("%s: %i\n", link->Name, link->Value->Value);
							break;
						}
					}
				}
				else if( strcmp(commandBuffer, "dispall") == 0 ) {
					 int	len = strlen(argBuffer);
					for( link = gpLinks; link; link = link->Next ) {
						if( link->Name[0] && strncmp(link->Name, argBuffer, len) == 0 ) {
							printf("%s: %i\n", link->Name, link->Value->Value);
						}
					}
				}
				else {
					printf("Unknown command '%s'\n", commandBuffer);
				}
			}
		}
		
		RunSimulationStep(NULL);
	}
	
	// Swap back to main buffer
	printf("\x1B[?1047l");
	
	return 0;
}

void RunSimulationStep(tTestCase *Root)
{
	tLink	**links;
	tElement	**elements;
	
	if( Root ) {
		links = &Root->Links;
		elements = &Root->Elements;
	}
	else {
		links = &gpLinks;
		elements = &gpElements;
	}
	
	for( tLink *link = *links; link; link = link->Next )
	{
		ASSERT(link != link->Next);
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
		ASSERT(ele != ele->Next);
		ele->Type->Update( ele );
	}
	
	// Set values
	for( tLink *link = *links; link; link = link->Next )
	{
		ASSERT(link != link->Next);
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
		ASSERT(link != link->Next);
		link->Value->NDrivers = 0;
		if( link->Name[0] == '1' )
		{
			ASSERT( link->Value->Value );
			link->Value->Value = 1;
			link->Value->NDrivers = 1;
		}
		else if( link->Name[0] == '0' )
		{
			ASSERT( !link->Value->Value );
			link->Value->Value = 0;
			link->Value->NDrivers = 0;
		}
	}
}

void ShowDisplayItems(tDisplayItem *First)
{
	 int	i;
	 int	bHasDisplayed = 0;
	for( tDisplayItem *dispItem = First; dispItem; dispItem = dispItem->Next )
	{
		ASSERT(dispItem != dispItem->Next);
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

void ReadCommand(int MaxCmd, char *Command, int MaxArg, char *Argument)
{
	 int	i;
	char	ch;
	
	while( (ch = getchar()) == ' ' );
	if( ch == '\n' ) {
		//printf("ret %02x\n", ch);
		return ;
	}
	
	Argument[0] = '\0';
	i = 0;
	do {
		if( i + 1 < MaxCmd )
			Command[i++] = ch;
	} while( (ch = getchar()) != ' ' && ch != '\n' );
	Command[i] = '\0';
	
	while( ch == ' ' )	ch = getchar();
	//printf("%02x", ch);
	if( ch == '\n' ) {
		return ;
	}
	
	i = 0;
	do {
	//	printf("%02x", ch);
		if( i + 1 < MaxArg )
			Argument[i++] = ch;
	} while( (ch = getchar()) != '\n' );
	Argument[i] = '\0';
}

void SigINT_Handler(int Signum)
{
	Signum = 0;
	#if 1
	// Swap back to main buffer
	printf("\x1B[?1047l");
	exit(55);
	#else
	//gbRunSimulation = 0;
	gbStepping = 1;
	#endif
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
						printf("%i", val & (1 << i));
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

void CompressLinks(void)
{
	 int	nVal = 0, nLink = 0;
	tLink	*link;
	tElement	*ele;
	printf("Culling links...\n");
	
	// Unify
	for( link = gpLinks; link; link = link->Next )
		link->Backlink = NULL;
	for( link = gpLinks; link; link = link->Next )
	{
		nLink ++;
		if(link->Backlink)	continue;	// Skip ones already done
		for( tLink *link2 = link; link2; link2 = link2->Next )
		{
			if(link2->Backlink)	continue;
			if(link2->Value != link->Value)	continue;
			link2->Backlink = link;	// Make backlink non-zero
		}
		nVal ++;
	}
	printf("%i values across %i links\n", nVal, nLink);
	
	// Update elements
	for( ele = gpElements; ele; ele = ele->Next )
	{
		for( int i = 0; i < ele->NInputs; i ++ )
			ele->Inputs[i] = ele->Inputs[i]->Backlink;
		for( int i = 0; i < ele->NOutputs; i ++ )
			ele->Outputs[i] = ele->Outputs[i]->Backlink;
	}
	// Update display conditions
	void _compactList(tList *list) {
	//	for( int i = 0; i < list->NItems; i ++ )
	//		list->Items[i] = list->Items[i]->Backlink;
	}
	for( tDisplayItem *disp = gpDisplayItems; disp; disp = disp->Next )
	{
		_compactList(&disp->Condition);
		_compactList(&disp->Values);
	}
	for( tTestCase *test = gpTests; test; test = test->Next )
	{
		for( tAssertion *a = test->Assertions; a; a = a->Next )
		{
			_compactList(&a->Condition);
			_compactList(&a->Values);
			_compactList(&a->Expected);
		}
		for( tDisplayItem *disp = test->DisplayItems; disp; disp = disp->Next )
		{
			_compactList(&disp->Condition);
			_compactList(&disp->Values);
		}
	}
	
	// Find and free unused
	// - NULL all backlinks as an "unused" flag
	for( link = gpLinks; link; link = link->Next )
		link->Backlink = NULL;
	for( ele = gpElements; ele; ele = ele->Next )
	{
		for( int i = 0; i < ele->NInputs; i ++ )
			ele->Inputs[i]->Backlink = (void*)1;
		for( int i = 0; i < ele->NOutputs; i ++ )
			ele->Outputs[i]->Backlink = (void*)1;
	}
	void _markList(tList *list) {
		for( int i = 0; i < list->NItems; i ++ )
			list->Items[i]->Backlink = (void*)1;
	}
	for( tDisplayItem *disp = gpDisplayItems; disp; disp = disp->Next )
	{
		_markList(&disp->Condition);
		_markList(&disp->Values);
	}
	for( tTestCase *test = gpTests; test; test = test->Next )
	{
		for( tAssertion *a = test->Assertions; a; a = a->Next )
		{
			_markList(&a->Condition);
			_markList(&a->Values);
			_markList(&a->Expected);
		}
		for( tDisplayItem *disp = test->DisplayItems; disp; disp = disp->Next )
		{
			_markList(&disp->Condition);
			_markList(&disp->Values);
		}
	}
	
	// Free any with a NULL backlink
	#if 0
	 int	nPruned = 0;
	tLink	*prev = (tLink*)&gpLinks;
	for( link = gpLinks; link; )
	{
		tLink *nextlink = link->Next;
		if(link->Backlink == NULL) {
			prev->Next = link->Next;
			free(link);
			nPruned ++;
		}
		else {
			prev = link;
		}
		link = nextlink;
	}
	printf("Pruned %i links\n", nPruned);
	#endif
}

void WriteCompiledVersion(const char *Path, int bBinary)
{
	 int	n_links = 0;
	 int	n_eletypes = 0;
	 int	n_elements = 0;
	FILE	*fp;
	tLink	*link;
	tElement	*ele;
	 int	i;
	
	if( !gbCompress )	CompressLinks();
	
	
	for( link = gpLinks; link; link = link->Next ) {
		link->Backlink = (void*)(intptr_t)n_links;	// Save
		n_links ++;
	}
	for( ele = gpElements; ele; ele = ele->Next )
		n_elements ++;
	for( tElementDef *ed = gpElementDefs; ed; ed = ed->Next )
		n_eletypes ++;

	void _Write16(FILE *fp, uint16_t val) {
		fputc(val & 0xFF, fp);
		fputc(val >> 8, fp);
	}
	
	fp = fopen(Path, "wb");
	
	if( bBinary ) {
		_Write16(fp, n_links);
		_Write16(fp, n_eletypes);
		_Write16(fp, n_elements);

		// Element type names
		for( tElementDef *ed = gpElementDefs; ed; ed = ed->Next )
			fwrite(ed->Name, 1, strlen(ed->Name)+1, fp);
	}
	else {
		fprintf(fp, "%04x %04x\n", n_links, n_elements);
	}
	
	// Element info
	for( ele = gpElements; ele; ele = ele->Next )
	{
		if( bBinary ) {
			_Write16(fp, ele->NInputs);
			_Write16(fp, ele->NOutputs);
//			_Write16(fp, ele->NParams);
			for( i = 0; i < ele->NInputs; i ++ )
				_Write16(fp, (intptr_t)ele->Inputs[i]->Backlink);
			for( i = 0; i < ele->NOutputs; i ++ )
				_Write16(fp, (intptr_t)ele->Outputs[i]->Backlink);
//			for( i = 0; i < ele->NParams; i ++ )
//				_Write32(fp, ele->Params[i]);
		}
		else {
			fprintf(fp,
				"%s %02x %02x %02x",
				ele->Type->Name, 0, ele->NInputs, ele->NOutputs
				);
			// TODO: Params
			for( i = 0; i < ele->NInputs; i ++ )
				fprintf(fp, " %04x", (int)(intptr_t)ele->Inputs[i]->Backlink);
			for( i = 0; i < ele->NOutputs; i ++ )
				fprintf(fp, " %04x", (int)(intptr_t)ele->Outputs[i]->Backlink);
			fprintf(fp, "\n");
		}
	}
	
	fclose(fp);
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

void ResolveLinks(tLink *First)
{
	for( tLink *link = First; link; link = link->Next )
	{
		// Expand n-deep linking
		while( link->Link && link->Link->Link ) {
			link->Link = link->Link->Link;
		}
		
		if( link->Link )
		{
			LinkValue_Ref(link->Link->Value);
			LinkValue_Deref(link->Value);
			link->Value = link->Link->Value;
		}
		
		// Zero out
		link->Value->NDrivers = 0;
		link->Value->Value = 0;
	}
}

void ResolveEleLinks(tElement *First)
{
	tLink	*link;	
	for( tElement *ele = First; ele; ele = ele->Next )
	{
		for( int i = 0; i < ele->NInputs; i ++ )
		{
			// Is it direct?
			if( !ele->Inputs[i]->Link )
				continue ;
			
			link = ele->Inputs[i]->Link;
			// TODO: Reference counting
			ele->Inputs[i] = link;
		}
		for( int i = 0; i < ele->NOutputs; i ++ )
		{
			if( !ele->Outputs[i]->Link )
				continue ;
			
			link = ele->Outputs[i]->Link;
			ele->Outputs[i] = link;
		}
	}

}

