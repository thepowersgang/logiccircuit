/*
 */
#include <Interface.Element.hpp>

class Element_XOR: public Element
{
public:
	void Update()
	{
		 int	out = 0;
		for( int i = 0; i < this->NInput; i++ )
		{
			out = out ^ (this->Inputs[i]->GetValue() != 0);
		}
		this->Outputs[0]->SetValue(out);
	}
};

