/*
 */
#include <Interface.Element.hpp>

class Element_NAND: public Element
{
public:
	void Update(void)
	{
		 int	out = 1;
		for( int i = 0; i < this->NInput; i++ )
		{
			out = out && this->InputValues[i];
		}
		this->OutputValues[0] = !out;
	}
	
};

