#include <Wt/WValidator>

namespace UserInterface {

class DirectoryValidator : public Wt::WValidator
{
	public:
		DirectoryValidator(Wt::WObject *parent = 0);

		Wt::WValidator::Result validate(const Wt::WString& input) const;

};

} // namespace UserInterface