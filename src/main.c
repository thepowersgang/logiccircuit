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
#include <compiled.h>
#include <assert.h>

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
 int	main(int argc, char *argv[]);
void	ReadCommand(int MaxCmd, char *Command, int MaxArg, char *Argument);
void	SigINT_Handler(int Signum);
void	CompressLinks(tTestCase *Root);
void	WriteCompiledVersion(const char *Path, int bBinary);
void	DumpList(const tList *List, int bShowNames);
void	CompileStatistics(tExecUnit *Unit);
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
const char	*gsTestName;
 int	gbTest_ShowDisplay = 0;
 int	gbDisableTests = 0;

// === CODE ===
int main(int argc, char *argv[])
{
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
	ADD_ELEDEF(MEMORY_DRAM);
	
	// Load Circuit file(s)
	for( int i = 1; i < argc; i ++ )
	{
		if( argv[i][0] == '-' )
		{
			if( strcmp(argv[i], "-links") == 0 )
				gbPrintLinks = 1;
			else if( strcmp(argv[i], "-stats") == 0 )
				gbPrintStats = 1;
			else if( strcmp(argv[i], "-notests") == 0 ) 
				gbDisableTests = 1;
			else if( strcmp(argv[i], "-test") == 0 )
				gbRunTests = 1;
			else if( strcmp(argv[i], "-onetest") == 0 ) {
				if(i + 1 == argc)	return -1;
				gbRunTests = 1;
				gsTestName = argv[++i];
			}
			else if( strcmp(argv[i], "-testdbg") == 0 )
				gbTest_ShowDisplay = 1;
			else if( strcmp(argv[i], "-count") == 0 ) {
				if(i + 1 == argc)	return -1;
				giSimulationSteps = atoi(argv[++i]);
			}
			else if( strcmp(argv[i], "-readbin") == 0 ) {
				if(i + 1 == argc)	return -1;
				ReadCompiledVersion(argv[++i], 1);
			}
			else if( strcmp(argv[i], "-readascii") == 0 ) {
				if(i + 1 == argc)	return -1;
				ReadCompiledVersion(argv[++i], 0);
			}
			else {
				// ERROR
				fprintf(stderr, "Unknown option '%s'\n", argv[i]);
				return -2;
			}
			continue ;
		}

		if( ParseFile(argv[i]) ) {
			return -1;
		}
	}

	// Print links
	// > Scan all links and find ones that share a value pointer 
	if( gbPrintLinks )
	{
		for( tLinkValue *val = gRootUnit.Values; val; val = val->Next )
		{
			 int	linkCount = 0;
			printf("(tValue)%p:", val);
			for( tLink *link = val->FirstLink; link; link = link->Next )
			{
				assert(link->Value == val);
				if( link->Name[0] )
					printf(" '%s'", link->Name);
				else
				#if 1
					printf(" (tLink)%p", link);
				#else	// HACK: Relies on change in build.c
					printf(" '%s'", link->Name+1);
				#endif
				linkCount ++;
			}
			printf(" (Used %i times)", linkCount);
			printf("\n");
		}
		
		return 0;
	}

	// Check for links that aren't set/read
	Sim_UsageCheck(&gRootUnit);
	for( tUnitTemplate *tpl = gpUnits; tpl; tpl = tpl->Next )
		Sim_UsageCheck(&tpl->Internals); 
//	for( tTestCase *test = gpTests; test; test = test->Next )
//		UsageCheck(&test->Internals);

	// TODO: Support saving tree to a file
	if( 1 ) {
		WriteCompiledVersion("cct.binary", 1);
		WriteCompiledVersion("cct.ascii", 0);
	}

	// Test case code	
	if( gbRunTests )
	{
		printf("=== Running tests ===\n");
		 int	nTests = 0;
		 int	nFailure = 0;
		for( tTestCase *test = gpTests; test; test = test->Next )
		{
			 int	steps_elapsed = 0;
			 int	bFailure = 0;

			if( gsTestName && strcmp(test->Name, gsTestName) != 0 )
				continue ;

			nTests ++;

			printf("Test '%s'", test->Name);
			fflush(stdout);
			tLink	*cc;
			tExecUnit *compiled_test = Sim_CreateMesh(test, &cc);
			printf("...");
			fflush(stdout);
			while( steps_elapsed != test->MaxLength && !bFailure )
			{
				Sim_RunStep(compiled_test);
				if( gbTest_ShowDisplay )
					Sim_ShowDisplayItems(compiled_test->DisplayItems);
				steps_elapsed ++;
			
				bFailure = Sim_CheckAssertions(compiled_test->Assertions);
				
				if( cc && GetLink(cc) )
					break;
			}
			if( bFailure == 1 ) {
				printf("\n- Test '%s' failed in %i steps\n", test->Name, steps_elapsed);
				nFailure ++;
			}
			else if( steps_elapsed == test->MaxLength ) {
				printf("\n- TIMED OUT (%i cycles)\n", steps_elapsed);
				nFailure ++;
			}
			else {
				printf(" PASS (%i cycles)\n", steps_elapsed);
			}
			
			Sim_FreeMesh(compiled_test);
		}
		
		printf("%i/%i Tests passed\n", nTests-nFailure, nTests);
		return 0;
	}

	printf("Compiling root...\n");
	tExecUnit *compiled_root = Sim_CreateMesh(NULL, NULL);

	if( gbPrintStats )
	{
		CompileStatistics(compiled_root);
		Sim_FreeMesh(compiled_root);
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
	for( int timestamp = 0; gbRunSimulation; timestamp ++ )
	{
		 int	breakPointFired = 0;

		if( giSimulationSteps && timestamp == giSimulationSteps )
			break;	
	
		// Clear drivers
		for( tLinkValue *val = compiled_root->Values; val; val = val->Next ) {
			val->NDrivers = 0;
			if( val == &gValue_Zero )
				val->Value = 0;
			if( val == &gValue_One ) {
				val->NDrivers = 1;
				val->Value = 1;
			}
		}

		// === Show Display ===
		if(!giSimulationSteps)
		{
			printf("\x1B[2J");
			printf("\x1B[H");
		}
		printf("---- %6i ----\n", timestamp);
		
		Sim_ShowDisplayItems(compiled_root->DisplayItems);
	
		// Check breakpoints
		breakPointFired = Sim_CheckBreakpoints(compiled_root);
		
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
					for( tLink *link = compiled_root->Links; link; link = link->Next ) {
						if( strcmp(link->Name, argBuffer) == 0 ) {
							printf("%s: %i\n", link->Name, link->Value->Value);
							break;
						}
					}
				}
				else if( strcmp(commandBuffer, "dispall") == 0 ) {
					 int	len = strlen(argBuffer);
					for( tLink *link = compiled_root->Links; link; link = link->Next ) {
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
		
		Sim_RunStep(compiled_root);
	}
	
	// Swap back to main buffer
	printf("\x1B[?1047l");

	Sim_FreeMesh(compiled_root);
	
	return 0;
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

void CompileStatistics(tExecUnit *Unit)
{
	 int	totalLinks = 0;
	 int	totalNamedLinks = 0;
	 int	totalValues = 0;
	 int	totalElements = 0;
	 int	totalSubunits = 0;
	
	printf("Gathering statistics...\n");
	for( tLink *link = Unit->Links; link; link = link->Next )
	{
		if(link->Name[0])
			totalNamedLinks ++;
		totalLinks ++;
	}
	for( tLinkValue *val = Unit->Values; val; val = val->Next )
		totalValues ++;
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
		totalElements ++;
	for( tExecUnitRef *ref = Unit->SubUnits; ref; ref = ref->Next )
		totalSubunits ++;
	

	printf("%i Links (%i named) for %i values\n", totalLinks, totalNamedLinks, totalValues);
	printf("%i atomic elements and %i sub-units\n", totalElements, totalSubunits);
}

#if 0
void CompressLinks(tTestCase *Root)
{
	 int	nVal = 0, nLink = 0;
	 int	i;
	tLink	*link;
	tElement	*ele;
	char	*links_merged;

	tLink	**links;
	tElement	**elements;
	tDisplayItem	**dispitems;
	
	GetLists(Root, &links, &elements, &dispitems);
	
//	printf("Culling links on %p\n", Root);
	
	// Unify
	for( link = *links; link; link = link->Next ) {
		link->Backlink = NULL;
		nLink ++;
	}
	links_merged = calloc(nLink, 1);
	assert(links_merged);

	// Set up backlinks on all links (merging common values)
	i = 0;
	for( link = *links; link; link = link->Next, i ++ )
	{
		 int j = i + 1;
		// Skip ones already done
		if(link->Backlink)
			continue;

		extern void gValue_Zero, gValue_One;
		if( strcmp(link->Name, "0") == 0 && link->Value != &gValue_Zero )
			fprintf(stderr, "Link %p is called 0 but doesn't use value (%p)\n", link, link->Value);
		if( strcmp(link->Name, "1") == 0 && link->Value != &gValue_One )
			fprintf(stderr, "Link %p is called 1 but doesn't use value (%p)\n", link, link->Value);

		link->Backlink = link;
		for( tLink *link2 = link->Next; link2; link2 = link2->Next, j ++ )
		{
			if(link2->Backlink)	continue;
			if(link2->Value != link->Value)	continue;
			link2->Backlink = link;	// Make backlink non-zero
			links_merged[j] = 1;
		}
		nVal ++;
	}
//	printf("%i values across %i links\n", nVal, nLink);
	
	// Update elements
	for( ele = *elements; ele; ele = ele->Next )
	{
		for( int i = 0; i < ele->NInputs; i ++ )
			ele->Inputs[i] = ele->Inputs[i]->Backlink;
		for( int i = 0; i < ele->NOutputs; i ++ )
			ele->Outputs[i] = ele->Outputs[i]->Backlink;
	}
	// Update display conditions
	void _compactList(tList *list) {
		for( int i = 0; i < list->NItems; i ++ ) {
			assert(list->Items[i]->Backlink);
			list->Items[i] = list->Items[i]->Backlink;
		}
	}
	for( tDisplayItem *disp = *dispitems; disp; disp = disp->Next )
	{
		_compactList(&disp->Condition);
		_compactList(&disp->Values);
	}
	if( Root )
	{
		for( tAssertion *a = Root->Assertions; a; a = a->Next )
		{
			_compactList(&a->Condition);
			_compactList(&a->Values);
			_compactList(&a->Expected);
		}
	}
	
	// Find and free unused
	// - NULL all backlinks as an "unused" flag
	for( link = *links; link; link = link->Next )
		link->Backlink = NULL;
	// Mark as non-null on used links
	for( ele = *elements; ele; ele = ele->Next )
	{
		for( int i = 0; i < ele->NInputs; i ++ )
			ele->Inputs[i]->Backlink = (void*)1;
		for( int i = 0; i < ele->NOutputs; i ++ )
			ele->Outputs[i]->Backlink = (void*)1;
	}
	void _markList(tList *list) {
		for( int i = 0; i < list->NItems; i ++ ) {
			assert(list->Items[i]);
			list->Items[i]->Backlink = (void*)1;
		}
	}
	for( tDisplayItem *disp = *dispitems; disp; disp = disp->Next )
	{
		_markList(&disp->Condition);
		_markList(&disp->Values);
	}
	if( Root )
	{
		for( tAssertion *a = Root->Assertions; a; a = a->Next )
		{
			_markList(&a->Condition);
			_markList(&a->Values);
			_markList(&a->Expected);
		}
	}
	
	// Free any with a NULL backlink and were merged
	 int	nPruned = 0;
	tLink	*prev = (tLink*)links;
	i = 0;
	for( link = *links; link; i ++)
	{
		tLink *nextlink = link->Next;
		// If not used, and was merged into the first shared
		if(link->Backlink == NULL && links_merged[i]) {
			prev->Next = link->Next;
			free(link);
			nPruned ++;
		}
		else {
			prev = link;
		}
		link = nextlink;
	}
//	printf("Pruned %i links\n", nPruned);

	free( links_merged );
}
#endif
