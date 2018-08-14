#include <ios>
#include <istream>

template<typename charT, typename traits>
std::basic_istream<charT,traits>&
skipline(std::basic_istream<charT,traits>& in){ //burn characters from a string until we hit a newline
	charT c;
	while(in.get(c) && c!='\n')
		;
	return in;
}
