/*
 */
#include <Interface.Element.hpp>

class Element_NOR: public Element
{
public:
	void Update(void)
	{
		 int	out = 0;
		for( int i = 0; i < this->NInput; i++ )
		{
			out = out || this->InputValues[i];
		}
		this->OutputValues[0] = !out;
	}
};

