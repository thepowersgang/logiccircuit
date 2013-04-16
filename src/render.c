/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 * 
 * render.c
 * - Circuit visualisation
 */

// === CODE ===
void Render_DrawBlock(tExecUnit *Unit, tBlock *Block, void *Buffer, int *W, int *H)
{
	// 1. Make list of used lines in this block
	// 2. Find all input / output lines (by usage)
	// 3. Locate rw lines that are used outside
	// 4. Locate lines that are double-set/read

	// 0. Convert elements into a compacted form (groups grouped with ranges)	

	// 1. Directly connected to input and output
	// 2. Directly connected to input or output
	// 3. Any others internally, working from the output
}
