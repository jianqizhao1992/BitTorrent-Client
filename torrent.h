#ifndef TORRENT_H_
#define TORRENT_H_

#include <sstream>
#include <iostream>
#include <map>
#include <cmath>
#include <string>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <deque>

class Torrent{
public:
	static int getTorrentMap(char*, std::map<std::string, std::string>&);
	static std::string readFile(std::string);
	static std::deque<std::string> readBencode(std::string, int);
	static int strtoi(std::string);
};


#endif /* TORRENT_H_ */
