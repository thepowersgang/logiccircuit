/*
 */
#ifndef _COMMON_H_
#define _COMMON_H_

#include <link.h>
#include <element.h>

#define _STR(x)	#x
#define STR(x)	_STR(x)

#define DEBUG_S(fmt, v...)	printf("%s:%i: "fmt"\n", __func__, __LINE__ ,## v); 

typedef struct sList	tList;
typedef struct sDisplayItem	tDisplayItem;
typedef struct sBreakpoint	tBreakpoint;
typedef struct sAssertion	tAssertion;
typedef struct sExecUnitRef	tExecUnitRef;
// No tExecUnit here
typedef struct sGroupDef	tGroupDef;
typedef struct sUnitTemplate	tUnitTemplate;

struct sList
{
	 int	NItems;
	tLink	**Items;
};

struct sBreakpoint
{
	tBreakpoint	*Next;
	tList	Condition;
	char	Label[];
};

struct sDisplayItem
{
	tDisplayItem	*Next;
	tList	Condition;	//!< Condition for the item to be shown
	tList	Values;	//!< List of values to show
	char	Label[];	//!< Display label
};

struct sAssertion
{
	struct sAssertion	*Next;
	tList	Condition;
	tList	Values;
	tList	Expected;
};

struct sExecUnitRef
{
	tExecUnitRef	*Next;
	tList	Inputs;
	tList	Outputs;
	struct sUnitTemplate	*Def;
};

typedef struct sExecUnit
{
	const char	*Name;
	tList	Inputs;
	tList	Outputs;
	
	tGroupDef	*Groups;
	 int	nLinks;
	tLink	*Links;
	tLink	*FirstNamedLink;

	tLinkValue	*Values;

	 int	nElements;
	tExecUnitRef	*SubUnits;
	struct	sElement	*Elements;
	tDisplayItem	*DisplayItems;
	tBreakpoint	*Breakpoints;
	tAssertion	*Assertions;
} tExecUnit;

typedef struct sTestCase
{
	struct sTestCase	*Next;

	tExecUnit	Internals;
	
	tLink	*CompletionCondition;
	 int	MaxLength;
	char	Name[];
} tTestCase;

/**
 * \brief Line array (group) definition structure
 */
struct sGroupDef
{
	tGroupDef	*Next;
	 int	Size;	//!< Size of the group
	char	Name[];	//!< Name
};

/**
 * \brief #defunit structure
 */
struct sUnitTemplate
{
	tUnitTemplate	*Next;
	tExecUnit	Internals;	
	 int	InstanceCount;	//!< Number of times the unit has been used
	char	Name[];
};

// Sim Engine
extern void	Sim_UsageCheck(tExecUnit *Root);
extern tExecUnit	*Sim_CreateMesh(tTestCase *Template, tLink **CompletionCond);
extern void	Sim_FreeMesh(tExecUnit *Unit);
extern void	Sim_RunStep(tExecUnit *Root);
extern void	Sim_ShowDisplayItems(tDisplayItem *First);
extern int	Sim_CheckBreakpoints(tExecUnit *Unit);
extern int	Sim_CheckAssertions(tAssertion *First);

// Everything else
extern int	Unit_DefineUnit(const char *Name);
extern int	Unit_AddSingleInput(const char *Name);
extern int	Unit_AddGroupInput(const char *Name, int Size);
extern int	Unit_AddSingleOutput(const char *Name);
extern int	Unit_AddGroupOutput(const char *Name, int Size);
extern int	Unit_CloseUnit(void);
extern int	Unit_IsInUnit(void);

extern int	Test_CreateTest(int MaxLength, const char *Name, int NameLength);
extern int	Test_AddAssertion(const tList *Condition, const tList *Values, const tList *Expected);
extern int	Test_AddCompletion(const tList *Condition);
extern int	Test_CloseTest(void);
extern int	Test_IsInTest(void);

extern tExecUnit	gRootUnit;
extern tUnitTemplate	*gpUnits;
extern int	gbDisableTests;
extern tLinkValue	gValue_Zero;
extern tLinkValue	gValue_One;

/**
 * \brief Add a display item
 * \return Pointer to the item
 */
extern tDisplayItem	*Build_AddDisplayItem(const char *Name, const tList *Condition, const tList *Values);

/**
 * \brief Add a breakpoint
 * \return Pointer to the breakpoint
 */
extern tBreakpoint	*Build_AddBreakpoint(const char *Name, const tList *Condition);

/**
 * \brief Create a line group
 */
extern void	Build_CreateGroup(const char *Name, int Size);

/**
 * \brief Free an allocated list
 */
extern void	List_Free(tList *List);
/**
 * \brief Append one list to another
 */
extern void	List_AppendList(tList *DestList, const tList *SourceList);
/**
 * \brief Append a line to a list
 */
extern void	List_AppendLine(tList *DestList, const char *Name);
/**
 * \brief Append a group to a list
 */
extern int	List_AppendGroup(tList *DestList, const char *Name);
/**
 * \brief Append a group item to a list
 */
extern int	List_AppendGroupItem(tList *DestList, const char *Name, int Index);
/**
 * \brief Merges the links from \a Src into \a Dest
 * Replaces the links in \a Dest with \a Src
 */
extern int	List_EquateLinks(tList *Dest, const tList *Src);
/**
 * \brief Create a gate/unit
 */
extern tList	*Build_ReferenceUnit(const char *Name, int NParams, const int *Params, const tList *Inputs);

extern tElement	*Build_DuplicateElement(const tElement *Element);


extern int	Build_IsConstValue(tLinkValue *Val);

#endif

