/*
 * LogicCircuit (Logic Gate Simulator)
 * - By John Hodge (thePowersGang)
 *
 * include/filerom.h
 * - Definitions used for the FILEROM unit
 */
#ifndef _FILEROM_H_
#define _FILEROM_H_

#define MAX_FILEROMS	8
#define MAX_ROMFILE_SIZE	(1024*1024)

extern int	giNumROMFiles;
extern size_t	gaROMFileSizes[MAX_FILEROMS];
extern void	*gaROMFileData[MAX_FILEROMS];

#endif

