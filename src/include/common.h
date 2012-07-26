/*
 */
#ifndef _COMMON_H_
#define _COMMON_H_

#include <link.h>

#define _STR(x)	#x
#define STR(x)	_STR(x)

#define ASSERT(cnd)	do{if(!(cnd)){fprintf(stderr,"ASSERT at "__FILE__":"STR(__LINE__)" ("#cnd" failed\n");exit(-1);}}while(0)

typedef struct sList	tList;
typedef struct sDisplayItem	tDisplayItem;
typedef struct sBreakpoint	tBreakpoint;

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

typedef struct sAssertion
{
	struct sAssertion	*Next;
	tList	Condition;
	tList	Values;
	tList	Expected;
} tAssertion;

typedef struct sTestCase
{
	struct sTestCase	*Next;
	
	tLink	*Links;
	struct sGroupDef	*Groups;
	struct sElement	*Elements;
	
	tAssertion	*Assertions;
	
	 int	MaxLength;
	char	Name[];
} tTestCase;

extern int	Unit_DefineUnit(const char *Name);
extern int	Unit_AddSingleInput(const char *Name);
extern int	Unit_AddGroupInput(const char *Name, int Size);
extern int	Unit_AddSingleOutput(const char *Name);
extern int	Unit_AddGroupOutput(const char *Name, int Size);
extern int	Unit_CloseUnit(void);
extern int	Unit_IsInUnit(void);

extern int	Test_CreateTest(int MaxLength, const char *Name, int NameLength);
extern int	Test_AddAssertion(const tList *Condition, const tList *Values, const tList *Expected);
extern int	Test_CloseTest(void);
extern int	Test_IsInTest(void);

/**
 * \brief Add a display item
 * \return Pointer to the item
 */
extern tDisplayItem	*AddDisplayItem(const char *Name, const tList *Condition, const tList *Values);

/**
 * \brief Add a breakpoint
 * \return Pointer to the breakpoint
 */
extern tBreakpoint	*AddBreakpoint(const char *Name, const tList *Condition);

/**
 * \brief Create a line group
 */
extern void	CreateGroup(const char *Name, int Size);

/**
 * \brief Free an allocated list
 */
extern void	List_Free(tList *List);
/**
 * \brief Append one list to another
 */
extern void AppendList(tList *DestList, tList *SourceList);
/**
 * \brief Append a line to a list
 */
extern void AppendLine(tList *DestList, const char *Name);
/**
 * \brief Append a group to a list
 */
extern int AppendGroup(tList *DestList, const char *Name);
/**
 * \brief Append a group item to a list
 */
extern int AppendGroupItem(tList *DestList, const char *Name, int Index);
/**
 * \brief Merges the links from \a Src into \a Dest
 * Replaces the links in \a Dest with \a Src
 */
extern int	MergeLinks(tList *Dest, tList *Src);
/**
 * \brief Create a gate/unit
 */
extern tList	*CreateUnit(const char *Name, int NParams, int *Params, tList *Inputs);

#endif

