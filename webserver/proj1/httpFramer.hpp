#ifndef HTTPFRAMER_HPP
#define HTTPFRAMER_HPP

#include <iostream>

class Framer {
public:
	void append(std::string chars);

	// Does this buffer contain at least one complete message?
	bool hasMessage() const;

	// Copy the first instruction in this buffer into the provided class
	std::string topMessage() const;

	// Pops off the first instruction from the buffer
	void popMessage();

	// prints the string to an ostream (useful for debugging)
	void printToStream(std::ostream& stream) const;

protected:
	// PUT ANY FIELDS/STATE HERE
	std::string buffer;
};

#endif // HTTPFRAMER_HPP
