#ifndef HELPER_H_
#define HELPER_H_

#include <fstream>
#include <iostream>

#include "Peer.h"

class Helper{
public:
	static void DieWithUserMessage(const char *msg, const char *detail);
	static void DieWithSystemMessage(const char *msg);
	static int writeFile(std::fstream *fs, void *buffer, int offset, const char *filename, long unsigned int len);
	static void LiveWithSystemMessage(const char *msg);
	static void LiveWithUserMessage(const char *msg, const char *detail);
	static int readFile(void *begin, char *fn, int offset, int len);
	static void parse_args(Peer &, int,  char **);
	static void __parse_local(Peer &, char *);
	static void __parse_peer(Peer &, int, char *);
	static void usage(FILE * file);
};

#endif /* HELPER_H_ */
