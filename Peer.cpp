#include "Peer.h"
#include "Helper.h"
#include "torrent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <deque>
#include <cstring>
#include <openssl/hmac.h> // need to add -lssl to compile
#include <openssl/sha.h>

const char Peer::key[] = {0xfa, 0xe2, 0x01, 0xd3, 0xba, 0xa9, 0x9b, 0x28, 0x72, 0x61, 0x5c, 0xcc, 0x3f, 0x28, 0x17, 0x0e};

Peer::Peer(){
	this -> server_sock = 0;
	this ->connected_number = 0;
	this->total_download = 0;
	this->total_upload = 0;
}

void Peer::readSock(int sock, void *buffer, int read_num, int *sum){
	void *writePoint = buffer;
	int sizeLeft = read_num;
	int readNum = 0;
	*sum = 0;
	while(readNum < sizeLeft) {
		sizeLeft = sizeLeft - readNum;
		writePoint = (void*)((char*)writePoint + readNum);
		readNum = recv(sock, (char*)buffer + readNum, sizeLeft, 0);
		if(readNum < 0)
	        Helper::DieWithSystemMessage("recv() failed");
	    if(readNum == 0)
	        break;
	    *sum = *sum + readNum;
	}
}

int Peer::getProgress(){
	unsigned int i;
	int count = 0;
	for(i = 0; i < this->bit_piece_num; i++){
		if(this->getBitValue(i) == 1){
			count++;
		}
	}
	return (100*count)/this->bit_piece_num;
}

int Peer::updateLocalInfo(char *host_name, unsigned short port){
	struct hostent * hostinfo;
	hostinfo = gethostbyname(host_name);
	if(!hostinfo){
		fprintf(stderr, "ERROR: Invalid host name %s", host_name);
		exit(1);
	}
	this -> local_addr.sin_family = hostinfo->h_addrtype;
	bcopy((char *) hostinfo->h_addr, (char *) &(this -> local_addr.sin_addr.s_addr), hostinfo->h_length);
	this -> local_addr.sin_port = htons(port);
	return 0;
}

void Peer::setPeerType(int type){
	this->type = type;
}

int Peer::updatePeerInfo(char *host_name, unsigned short port, int peer_num){
	struct hostent * hostinfo;
	hostinfo = gethostbyname(host_name);
	if(!hostinfo){
		fprintf(stderr, "ERROR: Invalid host name %s", host_name);
		exit(1);
	}
	this->peer_connection[peer_num].peer_addr.sin_family = hostinfo->h_addrtype;
	this->peer_connection[peer_num].exist = 1;
	bcopy((char *) hostinfo->h_addr, (char *) &(this->peer_connection[peer_num].peer_addr.sin_addr.s_addr), hostinfo->h_length);
	this->peer_connection[peer_num].peer_addr.sin_port = htons(port);
	return 0;
}

int Peer::initBindServerSock(){
	if((this->server_sock = socket(this->local_addr.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0){
	      Helper::DieWithSystemMessage("socket() failed");
	      return 1;
	}
	if(bind(this->server_sock, (struct sockaddr*) &this->local_addr, sizeof(this->local_addr)) < 0){
	      Helper::DieWithSystemMessage("bind() failed");
	      return 2;
	}
	return 0;
}

int Peer::listenServerSock(){
	if(listen(this->server_sock, MAX_CONNECTIONS) < 0){
		Helper::DieWithSystemMessage("listen() failed");
	    return -1;
	}
	if(this->addLocalIncomePoll(this->server_sock) != 0){
		Helper::DieWithUserMessage("listenServerSock()", "fail to add to local_income");
		return -2;
	}
	return 0;
}

int Peer::acceptClntSock(){
	int peer_num = this->getNextAvailPeerNum();
	if(peer_num < 0){
		Helper::LiveWithUserMessage("acceptClntSock()", "can't accept new connection since peer_connection table is full");
		return -2;
	}

	socklen_t clnt_addr_len = sizeof(this->peer_connection[peer_num].peer_addr);
	int sock = accept(this->server_sock, (sockaddr *) &(this->peer_connection[peer_num].peer_addr), &clnt_addr_len);
	if(sock < 0){
		Helper::LiveWithSystemMessage("accept() failed");
		return -1;
	}
	this->connected_number++;
	this->updatePeerName(peer_num);

	if(this->verbose){
		std::cout << "accepted a new connection from " << this->peer_connection[peer_num].peer_name << " on socket " << sock << std::endl;
	}
	if(this->log){
		char log[200];
		sprintf(log, "accepted a new connection from %s on socket %d", peer_connection[peer_num].peer_name, sock);
		this->writeToLogFile(log);
	}

	this->peer_connection[peer_num].active = 1;
	this->peer_connection[peer_num].exist = 1;
	this->peer_connection[peer_num].peer_sock = sock;
	this->addPeerIncomePoll(this->peer_connection[peer_num].peer_sock);
	return peer_num;
}

int Peer::getNextAvailPeerNum(){
	unsigned int i;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		if(this->peer_connection[i].exist == 0)
			return i;
	}
	return -1;
}

int Peer::sendSock(int dest_sock, void *buffer, int send_num, int &act_sent_num){
	act_sent_num = send(dest_sock, buffer, send_num, 0);
	if(act_sent_num < 0){
		Helper::LiveWithSystemMessage("send() failed");
		return -1;
	}
	else if(act_sent_num != send_num){
		Helper::LiveWithUserMessage("send()", "sent unexpected number of bytes");
		return 1;
	}
	else
		return 0;
}

std::string Peer::getMsgType(int type){
	if(type == 0)
		return "CHOKE";
	if(type == 1)
		return "UNCHOKE";
	if(type == 2)
		return "INTERESTED";
	if(type == 3)
		return "NOT INTERESTED";
	if(type == 4)
		return "HAVE";
	if(type == 5)
		return "BITFIELD";
	if(type == 6)
		return "REQUEST";
	if(type == 7)
		return "PIECE";
	if(type == 8)
		return "CANCEL";
	return "";
}

int Peer::checkHash(void *begin, unsigned int len){
	//suppose the hash length is 20, since we use sha1
	char hash_old[20];
	char hash_new[20];
	unsigned int hash_new_len;
	memcpy(hash_old, (char *)begin + len - 20, 20);
	//generate new hash value
	if(HMAC(EVP_sha1(), this->key, this->key_len, (const unsigned char*)begin, len, (unsigned char *)hash_new, &hash_new_len) == NULL){
		Helper::LiveWithUserMessage("HMAC()", "fail to generate mirror hash");
		return -1;
	}
	if(strcmp(hash_old, hash_new) != 0){
		Helper::LiveWithUserMessage("checkHash()", "your data's hash value is not correct");
		return 1;
	}
	else{
		return 0;
	}
}

int Peer::attachHash(const void *data_begin, unsigned int data_len, void *hash_begin){
	//we suppose the hash value takes 20 bytes, if the length is not 20 bytes, return 1, if fail to generate hash, return -1
	unsigned int hash_len;
	if(HMAC(EVP_sha1(), this->key, this->key_len, (const unsigned char*)data_begin, data_len, (unsigned char *)hash_begin, &hash_len) == NULL){
			Helper::LiveWithUserMessage("HMAC()", "fail to generate server-side hash");
			return -1;
	}
	else if(hash_len != 20){
		Helper::LiveWithUserMessage("HMAC()", "generate an abnormal sha1(length not 20)");
		return 1;
	}
	else
		return 0;
}

int Peer::getTorrentMap(){
	Torrent::getTorrentMap(this->torrent_file, this->torrent_map);
	return 0;
}

int Peer::getTorrentInfoHash(){
	std::string info_combine = "";
	info_combine.append(this->torrent_map["length"]);
	info_combine.append(this->torrent_map["name"]);
	info_combine.append(this->torrent_map["piece length"]);
	info_combine.append(this->torrent_map["pieces"]);

	Peer::attachHash(info_combine.c_str(), sizeof(info_combine.c_str()), this->info_hash);

	return 0;
}

int Peer::printTorrentMap(){
	std::map<std::string, std::string>::const_iterator it;
	std::cout << "---------------torrent content-- -------------" << std::endl;
	for(it = this->torrent_map.begin(); it != this->torrent_map.end(); ++it){

		std::cout << '"' << it->first << '"' << " : " << '"' << it->second << '"' << std::endl;

	}
	std::cout << "----------------------------------------------" << std::endl;
	return 0;
}

int Peer::calcLocalId(){
	unsigned char data[20];
	memset(data, 0, 20);

	int len_addr = sizeof(this->local_addr.sin_addr.s_addr);
	int len_port = sizeof(this->local_addr.sin_port);
	memcpy(data, (unsigned char *)&(this->local_addr.sin_addr.s_addr), len_addr);
	memcpy(data + len_addr, (char *)&(this->local_addr.sin_port), len_port);

	SHA1((unsigned char *) data, len_addr + len_port, (unsigned char *)this->local_id);
	return 0;
}

int Peer::calcPeerId(int peer_num){
	unsigned char data[20];
	memset(data, 0, 20);

	int len_addr = sizeof(this->peer_connection[peer_num].peer_addr.sin_addr.s_addr);
	int len_port = sizeof(this->peer_connection[peer_num].peer_addr.sin_port);

	memcpy(data, (unsigned char *)&(this->peer_connection[peer_num].peer_addr.sin_addr.s_addr), len_addr);
	memcpy(data + len_addr, (char *)&(this->peer_connection[peer_num].peer_addr.sin_port), len_port);

	SHA1((unsigned char *) data, len_addr + len_port, (unsigned char *)this->peer_connection[peer_num].peer_id);

	//this->peer_connection[peer_num].peer_id[20] = '\0';
	return 0;
}

int Peer::assemSeederHandShake(){
	char hs_0 = 19;
	std::string hs_1_19 = "BitTorrent Protocol";
	memcpy(this->hand_shake, &hs_0, 1);
	memcpy(this->hand_shake+1, hs_1_19.c_str(), 19);
	memset(this->hand_shake+20, 0, 8);
	memcpy(this->hand_shake+28, this->info_hash, 20);
	memcpy(this->hand_shake+48, this->local_id, 20);
	return 0;
}

int Peer::assemLeecherHandShake(int peer_num){
	char hs_0 = (char)19;
	char hs_1_19[] = "BitTorrent Protocol";
	memcpy(this->hand_shake, &hs_0, 1);
	memcpy(this->hand_shake+1, hs_1_19, 19);
	memset(this->hand_shake+20, 0, 8);
	memcpy(this->hand_shake+28, this->info_hash, 20);
	memcpy(this->hand_shake+48, this->peer_connection[peer_num].peer_id, 20);
	//test
	//std::cout << "print the head_5 of leecher handshake" << std::endl;
	//unsigned int head_4;
	//memcpy(&head_4, this->hand_shake, 4);
	//printf("%c\n", hand_shake[4]);
	//while(1){

	//}
	//test
	return 0;
}

int Peer::initBitFieldWithZero(){
	int block_num = 10;
	int i, j;
	for(i = 0; i < block_num; ++i)
		for(j = 0; j < 8; ++j)
			this->setBitField(8*i+j, 0);
	return 0;
}

int Peer::initBitFieldContactWithZero(){
	int block_num = 10;
	int i, j;
	for(i = 0; i < block_num; ++i)
		for(j = 0; j < 8; ++j){
			this->setBitFieldContact(8*i+j, 0);
		}
	return 0;
}

int Peer::initBitFieldWithOne(){
	int block_num = 10;
	int i, j;
	for(i = 0; i < block_num; ++i)
		for(j = 0; j < 8; ++j)
			this->setBitField(8*i+j, 1);
	return 0;
}

int Peer::setBitField(int bit_chunk_num, int value){
	int block_num = bit_chunk_num/8;
	int local_num = bit_chunk_num%8;
	if(value == 1){
		this->bit_field[block_num] |= 1<<local_num;
	}
	else{
		this->bit_field[block_num] &= ~(1<<local_num);
	}
	return 0;
}

int Peer::setBitFieldContact(int index, int value){
	int block_num = index/8;
	int local_num = index%8;
	if(value == 1){
		this->bit_field_contact[block_num] |= 1<<local_num;
	}
	else{
		this->bit_field_contact[block_num] &= ~(1<<local_num);
	}
	return 0;
}

bool Peer::getBitValue(int index){
	int block_num = index/8;
	int local_num = index%8;
	bool bit = this->bit_field[block_num] & (1<<local_num);
	return bit;
}

bool Peer::getBitContactValue(int index){
	int block_num = index / 8;
	int local_num = index % 8;
	bool bit = this->bit_field_contact[block_num] & (1 << local_num);
	return bit;
}

int Peer::initStartTime(){
	time_t timer;
	struct tm y2k;

	time(&timer);
	y2k.tm_hour = 0; y2k.tm_min = 0; y2k.tm_sec = 0;
	y2k.tm_year = 100; y2k.tm_mon = 0; y2k.tm_mday = 1;

	this->start_time = difftime(timer, mktime(&y2k));

	return 0;
}

double Peer::getCurrentTime(){
	time_t timer;
	struct tm y2k;

	time(&timer);
	y2k.tm_hour = 0; y2k.tm_min = 0; y2k.tm_sec = 0;
	y2k.tm_year = 100; y2k.tm_mon = 0; y2k.tm_mday = 1;

	return difftime(timer, mktime(&y2k)) - this->start_time;
}

int Peer::assemUnChokeMsg(char *begin, int &len){
	unsigned int length = 1;
	unsigned char type = '1';
	memcpy(begin, &length, sizeof(unsigned int));
	memcpy(begin+sizeof(unsigned int), &type, sizeof(unsigned char));
	if((len = sizeof(unsigned int) + sizeof(unsigned char)) != 5){
		Helper::LiveWithUserMessage("assemUnChokeMsg()", "unchoke Msg has a length not 5");
	}
	return 0;
}

int Peer::assemInterestMsg(char *begin, int &len){
	unsigned int length = 1;
	unsigned char type = '2';
	memcpy(begin, &length, sizeof(unsigned int));
	memcpy(begin+sizeof(unsigned int), &type, sizeof(unsigned char));
	if((len = sizeof(unsigned int) + sizeof(unsigned char)) != 5){
		Helper::LiveWithUserMessage("assemInterestMsg()", "Interest Msg has a length not 5");
	}
	return 0;
}

int Peer::assemHaveMsg(unsigned int index, char *begin, int &len){
	unsigned int length = 5;
	unsigned char type = '4';
	memcpy(begin, &length, sizeof(unsigned int));
	memcpy(begin+sizeof(unsigned int), &type, sizeof(unsigned char));
	memcpy(begin+sizeof(unsigned int) + sizeof(unsigned char), &index, sizeof(unsigned int));
	if((len = sizeof(unsigned int) + sizeof(unsigned char) + sizeof(unsigned int)) != 9){
		Helper::LiveWithUserMessage("assemHaveMsg()", "Have Msg has a length not 9");
	}
	return 0;
}

int Peer::assemBitFieldMsg(char *begin, int &len){
	unsigned int length = 1 + sizeof(this->bit_field);
	unsigned char type = '5';
	memcpy(begin, &length, sizeof(unsigned int));
	memcpy(begin+sizeof(unsigned int), &type, sizeof(unsigned char));
	memcpy(begin+sizeof(unsigned int) + sizeof(unsigned char), &(this->bit_field), sizeof(this->bit_field));
	len = sizeof(unsigned int) + sizeof(unsigned char) + sizeof(this->bit_field);
	return 0;
}

int Peer::assemRequestMsg(char *msg_begin, unsigned int index, unsigned int begin, unsigned int length, int &len){
	unsigned int msg_length = 1 + 3 * sizeof(unsigned int);
	unsigned char type = '6';
	memcpy(msg_begin, &msg_length, sizeof(unsigned int));
	memcpy(msg_begin+sizeof(unsigned int), &type, sizeof(unsigned char));
	memcpy(msg_begin+sizeof(unsigned int)+sizeof(unsigned char), &index, sizeof(unsigned int));
	memcpy(msg_begin+2*sizeof(unsigned int)+sizeof(unsigned char), &begin, sizeof(unsigned int));
	memcpy(msg_begin+3*sizeof(unsigned int)+sizeof(unsigned char), &length, sizeof(unsigned int));
	len = 4*sizeof(unsigned int) + sizeof(unsigned char);
	return 0;
}

int Peer::assemPieceMsg(char *begin, void *buffer, unsigned int write_len, unsigned int index, unsigned int offset, int &len){
	unsigned int msg_length = 1 + 2*sizeof(unsigned int) + write_len;
	unsigned char type = '7';
	memcpy(begin, &msg_length, sizeof(unsigned int));
	memcpy(begin+sizeof(unsigned int), &type, sizeof(unsigned char));
	memcpy(begin+sizeof(unsigned int)+sizeof(unsigned char), &index, sizeof(unsigned int));
	memcpy(begin+2*sizeof(unsigned int)+sizeof(unsigned char), &offset, sizeof(unsigned int));
	memcpy(begin+3*sizeof(unsigned int)+sizeof(unsigned char), buffer, write_len);
	len = 3*sizeof(unsigned int) + sizeof(unsigned char) + write_len;
	return 0;
}

int Peer::msgInterpret(char *buffer){
	int type = atoi(&buffer[0]);
	if(type >= 0 && type <= 8)
		return type;
	else
		return -1;
}

int Peer::initSaveFileWritePointer(){
	this->out_save.open(this->save_file, std::ios::binary | std::ios::out | std::ios::in | std::fstream::trunc);
	return 0;
}

int Peer::initLogFileWritePointer(){
	this->out_log.open(this->log_file, std::ios::binary | std::ios::out | std::ios::in);
	return 0;
}

int Peer::initSourceFileReader(){
	this->source_file_reader = fopen(this->source_file, "rb");
	return 0;
}

int Peer::initLogFileWriter(){
	this->log_file_writer = fopen(this->log_file, "a");
	return 0;
}

int Peer::readSourceFile(void *begin, int offset, int len){
	if(this->source_file_reader == NULL)
		Helper::DieWithSystemMessage("File open error");
	fseek(this->source_file_reader, offset, SEEK_SET);
	int result = fread(begin, sizeof(char), len, this->source_file_reader);
	if(result != len){
		Helper::LiveWithSystemMessage("File read inconsistent number data");
		return -1;
	}
	return 0;
}

int Peer::writeToLogFile(const char* szString){
	fprintf(this->log_file_writer, "%s\n", szString);
	return 0;
}

int Peer::writeToSaveFile(void *buffer, int offset, long unsigned int len){
	Helper::writeFile(&this->out_save, buffer, offset, this->save_file, len);
	return 0;
}

int Peer::closeSaveFileWritePointer(){
	this->out_save.close();
	return 0;
}

int Peer::closeLogFileWritePointer(){
	this->out_log.close();
	return 0;
}

int Peer::initPeerConnection(){
	unsigned int i = 0;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		  this->peer_connection[i].active = 0;
		  this->peer_connection[i].choke = 1;
		  this->peer_connection[i].exist = 0;
		  this->peer_connection[i].interest = 0;
		  this->peer_connection[i].peer_sock = -1;
	}
	return 0;
}

int Peer::pollLocalIncome(int timeout){
	int rc = poll(this->local_income, 1, timeout);
	if(rc < 0){
		Helper::DieWithSystemMessage("poll() failed");
	}
	else if(rc == 0){ //poll time out without find a ready socket
		return 0;
	}
	else{
		if(this->local_income[0].revents != POLLIN){
			//std::cout << this->local_income[0].revents << std::endl;
			Helper::DieWithUserMessage("poll()", "Poll Event Error");
			return -3;
		}
		else
			return 1;
	}
	return -2;
}

int Peer::pollPeerIncome(int timeout){
	int i = 0;
	int inactive_num = 0;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		if(this->peer_connection[i].active == 0){
			inactive_num++;
		}
	}
	if(inactive_num >= MAX_CONNECTIONS){
		return -2;
	}

	int rc = poll(this->peer_income, MAX_CONNECTIONS, timeout);

	if(rc < 0){
		Helper::DieWithSystemMessage("poll() failed");
	}
	else if(rc > 0){
		int i = 0;
		int peer_num = -1;
		for(i = global_x; i < global_x + 5; i++){
			if(this->peer_income[i%5].revents == POLLIN){
				//std::cout << "trying to find peer number for socket number: " << this->peer_income[i%5].fd << std::endl;
				peer_num = getPeerNumFromSock(this->peer_income[i%5].fd);
				//std::cout << "have found peer_num: " << peer_num << std::endl;
				if(peer_num < 0){
					//Helper::LiveWithUserMessage("getPeerNumFromSock()", "can't find peer number using the sock number");
					return -1;
				}
				else{
					return peer_num;
				}
			}
		}
	}
	else if(rc == 0){ //poll time out without find a ready socket
		return -1;
	}
	return -3;
}

void Peer::updatePeerConnectionStatus(int timeout){
	if(poll(this->peer_income, MAX_CONNECTIONS, timeout) > 0){
		char buffer[32];
		int i = 0;
		for(i = 0; i < MAX_CONNECTIONS; i++){
			if(recv(this->peer_income[i].fd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0){
				//connection has been closed
				int closed_sock = peer_income[i].fd;
				int closed_peer_num = this->getPeerNumFromSock(closed_sock);

				this->peer_connection[closed_peer_num].active = 0;
				this->peer_connection[closed_peer_num].choke = 1;
				this->peer_connection[closed_peer_num].exist = 0;
				this->peer_connection[closed_peer_num].interest = 0;

				std::cout << std::endl;
				std::cout << "A client from " << this->peer_connection[closed_peer_num].peer_name << " has been disconnected from the server" << std::endl;
				std::cout << std::endl;


				this->connected_number--;
				close(closed_sock);
				peer_income[i].fd = -1;
				peer_income[i].events = POLLIN;
			}
		}
		return;
	}
}

void Peer::updatePeerName(int peer_num){
	char clntName[INET_ADDRSTRLEN];
	if(inet_ntop(AF_INET, &(this->peer_connection[peer_num].peer_addr.sin_addr.s_addr), clntName, sizeof(clntName))){
		sprintf(this->peer_connection[peer_num].peer_name, "%s:%d", clntName, ntohs(this->peer_connection[peer_num].peer_addr.sin_port));
	}
}

void Peer::updateLocalName(){
	char locName[INET_ADDRSTRLEN];
	if(inet_ntop(AF_INET, &(this->local_addr.sin_addr.s_addr), locName, sizeof(locName))){
		sprintf(this->local_name, "%s:%d", locName, ntohs(this->local_addr.sin_port));
	}
}

int Peer::initLocalPeerIncome(){
	unsigned int i = 0;

	this->local_income[0].fd = -1;
	this->local_income[0].events = POLLIN;

	for(i = 0; i < MAX_CONNECTIONS; i++){
		this->peer_income[i].fd = -1;
		this->peer_income[i].events = POLLIN;
	}
	return 0;
}

int Peer::sendHandShakeMsg(int sock){
	int act_sent_num;
	int send_num = sizeof(this->hand_shake);
	if(this->sendSock(sock, this->hand_shake, send_num, act_sent_num) != 0){
		Helper::LiveWithUserMessage("sendHandShake()", "wrong sent number or send() failed");
		return -1;
	}
	return 0;
}

int Peer::sendUnChokeMsg(int sock){
	char buffer[10];
	int len;
	int act_sent_len;
	this->assemUnChokeMsg(buffer, len);
	if(this->sendSock(sock, buffer, len, act_sent_len) != 0){
		Helper::LiveWithUserMessage("sendUnChokeMsg()", "wrong sent number or send() failed");
	}
	return 0;
}

int Peer::sendInterestMsg(int sock){
	char buffer[10];
	int len;
	int act_sent_len;
	this->assemInterestMsg(buffer, len);
	if(this->verbose){
		std::cout << "assemble interest msg with length: " << len << std::endl;
	}
	if(this->sendSock(sock, buffer, len, act_sent_len) != 0){
		Helper::LiveWithUserMessage("sendInterestMsg()", "wrong sent number or send() failed");
	}
	return 0;
}

int Peer::sendHaveMsg(int sock, unsigned int index){
	char buffer[10];
	int len;
	int act_sent_len;
	this->assemHaveMsg(index, buffer, len);
	if(this->sendSock(sock, buffer, len, act_sent_len) != 0){
		Helper::LiveWithUserMessage("sendHaveMsg()", "wrong sent number or send() failed");
	}
	return 0;
}

int Peer::sendBitField(int sock){
	char buffer[20];
	int len;
	int act_sent_len;
	this->assemBitFieldMsg(buffer, len);
	if(this->sendSock(sock, buffer, len, act_sent_len) != 0){
		Helper::LiveWithUserMessage("sendHaveMsg()", "wrong sent number or send() failed");
	}
	return 0;
}

int Peer::sendRequestMsg(int sock, unsigned int index, unsigned int offset, unsigned int data_len){
	char buffer[20];
	int len;
	int act_sent_len;
	this->assemRequestMsg(buffer, index, offset, data_len, len);
	if(this->sendSock(sock, buffer, len, act_sent_len) != 0){
		Helper::LiveWithUserMessage("sendRequestMsg()", "wrong sent number or send() failed");
	}
	return 0;
}

int Peer::sendPieceMsgs(int sock, unsigned int index, unsigned int offset, unsigned int request_len){
	char read_buffer[36000];
	char msg_buffer[36000];
	memset(read_buffer, 0, 36000);
	memset(msg_buffer, 0, 36000);
	int msg_len;
	int act_sent_len;
	int file_length = atoi(this->torrent_map["length"].c_str());
	int send_len;

	send_len = request_len < (file_length - index * this->piece_length) ? request_len : (file_length - index * this->piece_length);

	if(this->verbose){
			std::cout << "assembling data piece with index: " << index << " offset: " << offset << " length " << send_len << std::endl;
	}
	if(send_len > 36000){
		Helper::DieWithUserMessage("sendPieceMsgs()", "try to read from source file for more than buffer length");
	}
	//read from source file, try to prevent errors
	int file_read_times = 0;
	while(this->readSourceFile(read_buffer, index*this->piece_length + offset, send_len) < 0 && file_read_times < 5){
		if(this->verbose){
			std::cout << "a file reading problem occurs" << std::endl;
			std::cout << "trying to read again: " << " file offset " << index*this->piece_length + offset << " length " << send_len << std::endl;
		}
	}
	if(file_read_times >= 5){
		Helper::DieWithUserMessage("sendPieceMsgs()", "can't read from source file properly, the program stops");
	}
	//assemble a piece Message
	this->assemPieceMsg(msg_buffer, read_buffer, send_len, index, offset, msg_len);

	if(this->verbose){
			std::cout << "sending data piece with index: " << index << " offset: " << offset << " length: " << send_len << std::endl;
	}

	if(this->sendSock(sock, msg_buffer, msg_len, act_sent_len) != 0){
		Helper::LiveWithUserMessage("sendPieceMsgs()", "wrong sent number or send() failed");
	}
	if(this->verbose){
		std::cout << "send data piece with index: " << index << " offset: " << offset << " length: " << send_len << std::endl;
	}

	return 0;
}

int Peer::initSourceFile(){
	strncpy(this->source_file, this->torrent_map["name"].c_str(), FILE_NAME_MAX);
	return 0;
}

int Peer::initPieceBitNumber(){
	this->bit_p = 0;

	this -> piece_length = atoi(this->torrent_map["piece length"].c_str());
	this -> total_length = atoi(this->torrent_map["length"].c_str());

	if(this->total_length % this->piece_length == 0){
		this->piece_num = this->total_length / this->piece_length;
	}
	else{
		this->piece_num = this->total_length / this->piece_length + 1;
	}
	if(this->total_length % (this->piece_length / 8) == 0){
		this->bit_piece_num = this->total_length / (this->piece_length / 8);
	}
	else{
		this->bit_piece_num = this->total_length / (this->piece_length / 8) + 1;
	}
	return 0;
}

int Peer::getNextIndex(){

	unsigned int i = 0;
	for(i = 0; i < this->bit_piece_num; i++){
		if((this->getBitValue(i) == 0) && (this->getBitContactValue(i) == 0)){
			this->bit_p = i;
			return this->bit_p;
		}
	}
	return -1;
}
/*
int Peer::checkFileHash(){
	unsigned int i;
	unsigned int piece_length = atoi(this->torrent_map["piece length"].c_str());
	unsigned int length = atoi(this->torrent_map["length"].c_str());
	char file_data_buffer[piece_length + 1];
	char file_hash[20];
	char torrent_hash[20];
	memset(file_hash, 0, 20);
	memset(torrent_hash, 0, 20);
	for(i = 0; i < this->piece_num; i++){
		unsigned int p_len = (length-piece_length*i) <= (piece_length)?(length-piece_length*i):piece_length;
		std::cout << "read from file: " << this->save_file << std::endl;
		Helper::readFile(file_data_buffer, this->save_file, piece_length*i, p_len);
		this->attachHash(file_data_buffer, p_len, file_hash);
		memcpy(torrent_hash, this->torrent_map["pieces"].c_str() + i*20, 20);
		if(strcmp(torrent_hash, file_hash) != 0){
			this->setBitField(i, 0);
			return 1;
		}
		memset(file_hash, 0, 20);
		memset(torrent_hash, 0, 20);
	}
	return 0;
}
*/


int Peer::initPeerLocalIncomePoll(){
	int i;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		this->peer_income[i].fd = -1;
		this->peer_income[i].events = POLLIN;
	}
	this->local_income[0].fd = -1;
	this->local_income[0].events = POLLIN;
	return 0;
}

int Peer::addPeerIncomePoll(int sock){
	unsigned int i;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		if(this->peer_income[i].fd == -1){
			this->peer_income[i].fd = sock;
			this->peer_income[i].events = POLLIN;
			return 0;
		}
	}
	return -1;
}

int Peer::addLocalIncomePoll(int sock){
	if(this->local_income[0].fd == -1){
		this->local_income[0].fd = sock;
		return 0;
	}
	else
		return -1;
}

int Peer::connectToPeer(){
	int peer_num = -1;
	int i;

	//find a peer_num that 'exist' but not 'active'
	for(i = global_x; i < global_x + 5; i++){
		if(this->peer_connection[i%5].exist == 1 && this->peer_connection[i%5].active == 0){
			peer_num = i%5;
			break;
		}
	}
	if(peer_num == -1){
		//Helper::LiveWithUserMessage("connectToPeer()", "can't find a exist, non-active peer");
		return -1;
	}
	//create a stream socket using TCP
	int sock = socket(this->peer_connection[peer_num].peer_addr.sin_family, SOCK_STREAM, IPPROTO_TCP);
	if(sock < 0){
		Helper::LiveWithSystemMessage("Peer::connectionToPeer(): socket() failed");
		return 1;
	}
	//connection to dest
	if(connect(sock, (struct sockaddr *) &this->peer_connection[peer_num].peer_addr, sizeof(this->peer_connection[peer_num].peer_addr)) < 0){
		//Helper::LiveWithSystemMessage("Peer::connectionToPeer(): connect() failed");
		close(sock);
		return 2;
	}
	this->connected_number++;
	//add socket to peer_connection table
	this->peer_connection[peer_num].peer_sock = sock;
	if(this->verbose){
		std::cout << "CONNECTED TO A NEW SERVER:" << this->peer_connection[peer_num].peer_name << std::endl;
	}
	if(this->log){
		char log[200];
		sprintf(log, "[%d] SERVER CONNECTED: %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
		this->writeToLogFile(log);
	}
	//assemble a leecher handshake msg
	this->assemLeecherHandShake(peer_num);
	//send handshake msg
	if(this->sendHandShakeMsg(this->peer_connection[peer_num].peer_sock) != 0){
		Helper::LiveWithUserMessage("Peer::connectToPeer()", "sendHandShakeMsg() failed");
		return -2;
	}
	if(this->verbose){
		std::cout << "HANDSHAKE INIT TO " << this->peer_connection[peer_num].peer_name << std::endl;
	}
	if(this->log){
		char log[200];
		sprintf(log, "[%d] HANDSHAKE INIT TO %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
		this->writeToLogFile(log);
	}
	//add to peer_income
	if(this->addPeerIncomePoll(this->peer_connection[peer_num].peer_sock) != 0){
		Helper::LiveWithUserMessage("Peer::connectToPeer()", "addPeerIncomePoll() failed");
		return 3;
	};

	//set peer to 'active'
	this->peer_connection[peer_num].active = 1;

	return 0;
}

int Peer::getPeerNumFromSock(int sock){
	unsigned int i;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		if(this->peer_connection[i].peer_sock == sock)
			return i;
	}
	return -1;
}

void Peer::standardOutput(){
	std::cout << "File: " << this->torrent_map["name"] << " Progress: " << this->getProgress() << "% " << "Peers: " << this->connected_number << " Downloaded: " << this->total_download/1024 << " KB Uploaded: " << this->total_upload/1024 << " KB" << std::endl;
}

void Peer::printPeer(){
	  int i;
	  for(i = 0; i < MAX_CONNECTIONS; i++){
		  if(this->peer_connection[i].exist == 1)
			  std::cout << inet_ntoa(this->peer_connection[i].peer_addr.sin_addr);
		  	  std::cout << ":";
		  	  std::cout << this->peer_connection[i].peer_addr.sin_port <<std::endl;
	  }
}

void Peer::printPeerConnection(){
	int i;
	for(i = 0; i < MAX_CONNECTIONS; i++){
		std::cout << "   active: " << this->peer_connection[i].active;
		std::cout << "   exist: " << this->peer_connection[i].exist;
		std::cout << "   socket:" << this->peer_connection[i].peer_sock;
		std::cout << std::endl;
	}
}

void Peer::peerClose(int peer_num){
	close(this->peer_connection[peer_num].peer_sock);  //download completed, close the connection
	this->peer_connection[peer_num].choke = 1;
	this->peer_connection[peer_num].active = 0;
	this->peer_connection[peer_num].exist = 0;
	this->closeLogFileWritePointer();
	this->closeSaveFileWritePointer();
}

int Peer::incomeHandler(int peer_num){
	int sock = this->peer_connection[peer_num].peer_sock;
	int read_len;
	unsigned int head_4_bytes;
	char buffer[36000];
	memset(buffer, 36000, 0);
	readSock(sock, &head_4_bytes, 4, &read_len);

	readSock(sock, buffer, 1, &read_len);

	if(buffer[0] <= '8' && buffer[0] >= '0'){                        //if the fifth char of the message indicate a normal message type

		if(this->log == 1){
			char log[200];
			sprintf(log, "[%d] RECV A %s MSG FROM %s", (int)this->getCurrentTime(), this->getMsgType(buffer[0]).c_str(), this->peer_connection[peer_num].peer_name);
			this->writeToLogFile(log);
		}
		unsigned int len = head_4_bytes;
		if(len > 1)
			readSock(sock, buffer+1, len - 1, &read_len);
		this->msgHandler(buffer, len, peer_num);
		return 0;
	}
	else if(buffer[0] == 'T'){
		readSock(sock, buffer+1, 15, &read_len);
		char head_20[20];
		char handshake_head_20[20];
		char hs_0 = (char)19;
		char hs_1_19[] = "BitTorrent Protocol";
		memcpy(handshake_head_20, &hs_0, 1);
		memcpy(handshake_head_20+1, hs_1_19, 19);
		memcpy(head_20, &head_4_bytes, 4);
		memcpy(head_20+4, buffer, 16 );
		handshake_head_20[20] = '\0';
		head_20[20] = '\0';

		if(strcmp(head_20, handshake_head_20) == 0)  //we have a handshake message
		{
			//test
			if(this->verbose){
				std::cout << "RECV A HANDSHAKE MSG FROM " << this->peer_connection[peer_num].peer_name << std::endl;
			}
			if(this->log){
				char log[200];
				sprintf(log, "[%d] RECV A HANDSHAKE MSG FROM %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
				this->writeToLogFile(log);
			}
			//test
			memset(buffer, 0, sizeof(buffer));
			readSock(sock, buffer, 8, &read_len);
			readSock(sock, buffer, 20, &read_len);
			readSock(sock, buffer+20, 20, &read_len);
			if(this->type == 0){
				this->assemSeederHandShake();
				this->sendHandShakeMsg(sock);
				return 0;
			}
			else{
				//std::cout<<"leecher reaceived a send-back handshake message" << std::endl;
				char info_hash[20];
				char id[20];
				char info_hash_local[20];
				char id_local[20];
				memcpy(info_hash, buffer, 20);
				memcpy(id, buffer+20, 20);
				memcpy(info_hash_local, this->info_hash, 20);
				memcpy(id_local, this->peer_connection[peer_num].peer_id, 20);

				info_hash_local[20] = '\0';
				info_hash[20] = '\0';
				id[20] = '\0';
				id_local[20] = '\0';

				int check_info_hash;
				int check_id;


				check_info_hash = strcmp(info_hash, info_hash_local);
				check_id = strcmp(id, id_local);
				if(check_info_hash != 0 || check_id != 0){   //if the info_hash or id information don't match the handshake, the leecher will disconnect
					if(this->verbose){
						std::cout << "HANDSHAKE FAILED WITH " << this->peer_connection[peer_num].peer_name << std::endl;
					}
					if(this->log){
						char log[200];
						sprintf(log, "[%d] HANDSHAKE FAILED WITH peer: %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
						this->writeToLogFile(log);

					}
					close(this->peer_connection[peer_num].peer_sock);
					this->peer_connection[peer_num].active = 0;
					return 1;
				}
				else{								//if the info_hash and id infomation of the handshake match, the leecher will do..
					if(this->verbose){
						std::cout << "HANDSHAKE FAILED WITH " << this->peer_connection[peer_num].peer_name << std::endl;
					}
					if(this->log){
						char log[200];
						sprintf(log, "[%d] HANDSHAKE SUCCEED WITH peer: %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
						this->writeToLogFile(log);
					}
					if(this->verbose){
						std::cout << "Checking if the client is intersted in the file" << std::endl;
					}
					if(this->getNextIndex() >= 0){
						if(this->verbose){
							std::cout << "SEND INTERESTED MSG TO " << this->peer_connection[peer_num].peer_name << std::endl;
						}
						if(this->log){
							char log[200];
							sprintf(log, "[%d] SEND INTERESTED MSG TO %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
							this->writeToLogFile(log);
						}
						this->sendInterestMsg(this->peer_connection[peer_num].peer_sock);
					}
					else{
						std::cout << "NOT INTERESTED IN THE FILE, DISCONNECT" << std::endl;
						if(this->log){
							char log[200];
							sprintf(log, "[%d] NOT INTERESTED IN THE FILE, DISCONNECT FROM peer: %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
							this->writeToLogFile(log);
						}
						this->peerClose(peer_num);
						exit(0);
					}

				}
			}
		}
		else{
			Helper::LiveWithUserMessage("incomeHandler()", "encounter an unknow income message(expecting a handshake)");
			return -1;
		}
	}
	else{
		Helper::LiveWithUserMessage("incomeHandler()", "encounter an unknow income message");
		return -2;
	}
	return -3;
}


int Peer::msgHandler(char *buffer, unsigned int len, int peer_num){
	int msg_type = this->msgInterpret(buffer);
	if(this->type == 0 && this->peer_connection[peer_num].active == 1)    						//rules for seeder
	{
		if(msg_type == BT_HAVE){
			//ignore
			return 0;
		}
		else if(msg_type == BT_INTERESTED){
			if(this->verbose){

				std::cout << "got an interested message from socket: " << this->peer_connection[peer_num].peer_sock << std::endl;

				std::cout << "sending unchoke message to socket: " << this->peer_connection[peer_num].peer_sock << std::endl;

			}
			this->sendUnChokeMsg(this->peer_connection[peer_num].peer_sock);
			return 0;
		}
		else if(msg_type == BT_REQUEST){
			unsigned int index;
			unsigned int offset;
			unsigned int request_len;

			memcpy(&index, buffer+1, sizeof(unsigned int));
			memcpy(&offset, buffer+1+sizeof(unsigned int), sizeof(unsigned int));
			memcpy(&request_len, buffer+1+2*sizeof(unsigned int), sizeof(unsigned int));

			if(this->verbose){
				std::cout << "received request msg for index: " << index << " offset " << offset << " length " << request_len << std::endl;
			}
			if(this->log){
				char log[200];
				sprintf(log, "[%d] RECV REQUEST MSG FROM %s INDEX %d OFFSET %d LENGTH %d", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, index, offset, request_len);
				this->writeToLogFile(log);
			}

			//check if the request is valid, if not valid modify it to make it valid: such as request part exceeds length limit
			if(index*this->piece_length + offset + request_len > this->total_length){
				if(this->verbose){
					std::cout << "---received a invalid request, modify or ignore it" << std::endl;
				}
				if(index*this->piece_length + offset < this->total_length){
					if(this->verbose){
						std::cout << "---have found a way to modify it: cut the request length" << std::endl;
					}
					int modify_piece_length = this->total_length - index * this->piece_length - offset;
					this->sendPieceMsgs(this->peer_connection[peer_num].peer_sock, index, offset, modify_piece_length);

					std::cout << "log status:" << this->log << std::endl;

					if(this->log){
						char log[200];
						sprintf(log, "[%d] SEND PIECE MSG TO %s FOR INDEX %d OFFSET %d LENGTH %d", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, index, offset, modify_piece_length);
						this->writeToLogFile(log);
					}


					this->total_upload += modify_piece_length;
					return 0;
				}
				else{
					std::cout << "---have encountered an unmodifiable request, ignore the request" << std::endl;
					return 0;
				}
			}

			if(this->log){
					char log[200];
					sprintf(log, "[%d] SEND PIECE MSG TO %s FOR INDEX %d OFFSET %d LENGTH %d", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, index, offset, request_len);
					this->writeToLogFile(log);
			}


			this->sendPieceMsgs(this->peer_connection[peer_num].peer_sock, index, offset, request_len);
			this->total_upload += request_len;

			return 0;
		}
	}
	else if(this->type == 1 && this->peer_connection[peer_num].active == 1)             		//rules for leecher
	{

		if(msg_type == BT_UNCHOKE){
			if(this->verbose){
				std::cout << "received an unchoke msg from " << this->peer_connection[peer_num].peer_name << std::endl;
			}

			if(this->log){
				char log[200];
				sprintf(log, "[%d] RECV UNCHOKE MSG FROM %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
				this->writeToLogFile(log);
			}

			if(this->log){
				char log[200];
				sprintf(log, "[%d] SEND REQUEST MSG TO %s FOR INDEX %d OFFSET %d LENGTH %d", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, this->bit_p/8, (this->bit_p%8)*(this->piece_length/8), this->piece_length/8);
				this->writeToLogFile(log);
			}
			this->peer_connection[peer_num].choke = 0;

			//index = this->getNextIndex();
			if(this->getNextIndex() >= 0){
				if(this->verbose){
					std::cout << "sending request msg for index " << this->bit_p/8  << " offset "<< (bit_p%8)*(this->piece_length/8) << " length " << this->piece_length/8 << std::endl;
				}
				if(this->log){
					char log[200];
					sprintf(log, "[%d] SEND REQUEST MSG TO %s INDEX %d OFFSET %d LENGTH %d", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, this->bit_p/8, (bit_p%8)*(this->piece_length/8), piece_length/8);
					this->writeToLogFile(log);
				}
				this->sendRequestMsg(this->peer_connection[peer_num].peer_sock, this->bit_p / 8, (bit_p%8)*(this->piece_length/8), this->piece_length/8);
				setBitFieldContact(this->bit_p, 1);

				if(this->verbose){
					std::cout << "sent request msg successfully for index " << this->bit_p/8  << " offset "<< (bit_p%8)*(this->piece_length/8) << " length " << this->piece_length/8 << std::endl;
				}

			}
			return 0;
		}
		else if(msg_type == BT_PIECE){
			//when a piece message is received
			int index;
			unsigned int offset;
			memcpy(&index, buffer+1, sizeof(unsigned int));
			memcpy(&offset, buffer+1+sizeof(unsigned int), sizeof(unsigned int));
			if(this->verbose){
				std::cout << "RECV PIECE MSG FROM " << this->peer_connection[peer_num].peer_name << " INDEX " << index << " OFFSET " << offset << " LENGTH " << len-1-2*sizeof(unsigned int) << std::endl;
			}
			if(this->log){
				char log[200];
				sprintf(log, "[%d] RECV PIECE MSG FROM peer: %s INDEX %d OFFSET %d LENGTH %lu", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, index, offset, len-1-2*sizeof(unsigned int));
				this->writeToLogFile(log);
			}
			//write to file and set bitfield
			this->writeToSaveFile(buffer+1+2*sizeof(unsigned int), index*this->piece_length+offset, len-1-2*sizeof(unsigned int));
			this->total_download += len-1-2*sizeof(unsigned int);
			this->setBitField(index*8 + 8*offset/this->piece_length, 1);


			if(this->getNextIndex() >= 0){

				this->sendRequestMsg(this->peer_connection[peer_num].peer_sock, this->bit_p/8, (this->bit_p%8)*(this->piece_length/8), this->piece_length/8);
				setBitFieldContact(this->bit_p, 1);
				if(this->verbose){
					std::cout << "sent request msg for index " << this->bit_p/8 << " offset " << (this->bit_p%8)*(this->piece_length/8) << " length " <<  this->piece_length/8 << std::endl;
				}
				if(this->log){
					char log[200];
					sprintf(log, "[%d] SEND REQUEST MSG TO %s FOR INDEX %d OFFSET %d LENGTH %d", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name, this->bit_p/8, (this->bit_p%8)*(this->piece_length/8), this->piece_length/8);
					this->writeToLogFile(log);
				}
			}
			else{
				std::cout << "DOWNLOAD COMPLETE, DISCONNECT FROM " << this->peer_connection[peer_num].peer_name << std::endl;

				if(this->log){
					char log[200];
					sprintf(log, "[%d] DOWNLOAD COMPLETE, DISCONNECT FROM %s", (int)this->getCurrentTime(), this->peer_connection[peer_num].peer_name);
					this->writeToLogFile(log);
				}

				this->peerClose(peer_num);
				exit(0);
			}
		}
	}
	return 0;
}
