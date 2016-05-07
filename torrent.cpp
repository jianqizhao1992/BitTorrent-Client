#include "torrent.h"
#include <sstream>
#include <iostream>
#include <map>
#include <cmath>
#include <string>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <deque>


int filesize(const char* filename){
	std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
	return in.tellg();
}


int Torrent::getTorrentMap(char *fn, std::map<std::string, std::string> &torrent_map){
	std::deque<std::string> li;
	std::string torrent_str = readFile(fn);
	int length = filesize(fn);
	li = Torrent::readBencode(torrent_str, length);
	unsigned int i = 0;
	for(i = 0; i < li.size(); i = i + 2){
		torrent_map.insert(std::pair<std::string, std::string>(li.at(i), li.at(i+1)));
	}
	return 0;
}

std::string Torrent::readFile(std::string filename){
	std::ifstream file (filename.c_str());
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string torrent = buffer.str();
	return torrent;
}

int chartoi(char a){
	int ai = a - '0';
	return ai;
}

int Torrent::strtoi(std::string str){
	int str_len = strlen(str.c_str());
	int num = 0;
	int digit = 0;
	int i = 0;
	for(i = 0; i < str_len; i++){
		digit = chartoi(str[i]);
		num += digit * pow(10, (str_len - i -1));
	}
	return num;
}

std::deque<std::string> Torrent::readBencode(std::string benc, int len){
	std::deque<std::string> li;
	std::string parent = "";
	int p = 0;
	int num = 0;
	std::string num_str = "";
	int level = 0;
	while(p < len){
		if(benc[p] == 'd'){
			if(p != 0){
				parent = parent + "." + li.back();
				li.pop_back();

			}
			p++;
			level++;
		}
		else if('0' <= benc[p] && benc[p] <= '9'){
			while('0' <= benc[p] && benc[p] <= '9'){
				num_str = num_str + benc[p];
				p++;

			}

			if(benc[p] == ':'){
				p++;
				num = Torrent::strtoi(num_str.c_str());
				li.push_back(benc.substr(p, num));
				p += num;
				num = 0;
				num_str = "";
			}
			else{
				std::cout << "error: expecting : at location: " + p << std::endl;
			}
		}
		else if(benc[p] == 'i'){
			//cout << benc[p] << endl;
			p++;
			while('0' <= benc[p] && benc[p] <= '9'){
				num_str = num_str + benc[p];
				p++;
				//cout << "p_inside: " << p << endl;
			}
			if(benc[p] == 'e'){
				//cout << benc[p] << endl;
				p++;
				li.push_back(num_str.c_str());
				num = Torrent::strtoi(num_str.c_str());
				num = 0;
				num_str = "";
			}
			else{
				std::cout << "error: expecting 'e' at location " + p << std::endl;
				//return NULL;
			}
		}
		else if(benc[p] == 'e'){
			//cout << 'e' << endl;
			p++;
			level--;
		}
		else{
			//cout << benc[p] << endl;
			p++;
		}
	}
	if(p != len || level != 0){
		std::cout << "torrent corrupted" << std::endl;
	}
	//cout << "level: " << level << endl;
	return li;
}

/*
int main(){
	string filename = "download.mp3.torrent";
	map<string, string> result = map<string, string>();;

	getTorrentMap(filename, result);
	map<string, string>::const_iterator it;
	for(it = result.begin(); it != result.end(); ++it){
		cout << '"' << it->first << '"' << " : " << '"' << it->second << '"' << endl;
	}
}
*/
