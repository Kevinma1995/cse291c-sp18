#include <assert.h>
#include "httpFramer.hpp"

using namespace std;


void Framer::append(string chars)
{
	// PUT YOUR CODE HERE
	this->buffer.append(chars);
}

bool Framer::hasMessage() const
{
	// PUT YOUR CODE HERE
	return this->buffer.find("\r\n\r\n") != std::string::npos;
}

string Framer::topMessage() const
{
	// PUT YOUR CODE HERE
    size_t i = this->buffer.find("\r\n\r\n");
	return this->buffer.substr(0, i+2);
}

void Framer::popMessage()
{
	// PUT YOUR CODE HERE
	size_t i = this->buffer.find("\r\n\r\n");
	this->buffer = this->buffer.substr(i+4);
}

void Framer::printToStream(ostream& stream) const
{
	// (OPTIONAL) PUT YOUR CODE HERE--useful for debugging
	stream << this->buffer;
}
