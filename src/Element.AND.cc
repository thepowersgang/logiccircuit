/*
 */
#include <Interface.Element.hpp>

class Element_AND: public Element
{
public:
	void Update(void)
	{
		 int	out = 1;
		for( int i = 0; i < this->NInputs; i++ )
		{
			out = out && this->Inputs[i]->GetValue();
		}
		if( this->NOutputs )
			this->Outputs[0]->SetValue(out);
	}	
};


