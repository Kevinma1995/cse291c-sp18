#ifndef HTTPD_H
#define HTTPD_H

#include <string>

using namespace std;

void start_httpd(unsigned short port, string doc_root);

// Handle error with user msg
void DieWithUserMessage(const char *msg, const char *detail);
// Handle error with sys msg
void DieWithSystemMessage(const char *msg);

#endif // HTTPD_H
