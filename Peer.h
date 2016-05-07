#ifndef PEER_H_
#define PEER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <string>

/*Maximum file name size, to make things easy*/
#define FILE_NAME_MAX 1024

/*Maxium number of connections*/
#define MAX_CONNECTIONS 5

extern int global_x;

/*bt message type*/
#define BT_CHOKE 0
#define BT_UNCHOKE 1
#define BT_INTERESTED 2
#define BT_NOT_INTERESTED 3
#define BT_HAVE 4
#define BT_BITFILED 5
#define BT_REQUEST 6
#define BT_PIECE 7
#define BT_CANCEL 8

class connection_struc{
public:
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len;
	int peer_sock;
	char peer_name[16];
	char peer_id[20];
	int exist;
	int active;
	int choke;
	int interest;
};

class Peer{
private:
	/* for all */
	struct sockaddr_in local_addr;
	const static char key[];
	const static int key_len = 16;
	std::map<std::string, std::string> torrent_map;
	char info_hash[20];
	char local_id[20];
	char hand_shake[68];
	std::fstream out_save;
	std::fstream out_log;
	char bit_field[10];
	char bit_field_contact[10];
	unsigned int bit_p;
	unsigned int piece_num;
	unsigned int bit_piece_num;
	unsigned int piece_length;
	unsigned int total_length;

	/* for seeder */
	int server_sock;

	/* for leecher */


public:
	/* for all */
	int type; //0 indicates a seeder, 1 indicates a leecher
	int verbose; //0 not, 1 verbose
	char local_name[20]; 							//store local ip:port
	int log;
	char torrent_file[FILE_NAME_MAX];
	char save_file[FILE_NAME_MAX];
	char log_file[FILE_NAME_MAX];
	char source_file[FILE_NAME_MAX];
	FILE *source_file_reader;
	FILE *log_file_writer;
	struct pollfd local_income[1];              //a poll structure for a peer to listen on its local socket(now only for seeder on server_sock)
	struct pollfd peer_income[MAX_CONNECTIONS]; //poll structs for either seeder or leecher to listen on their peers' sockets
	unsigned int total_download;
	unsigned int total_upload;
	double start_time;							//store the start time in senconds format
	int connected_number;

	Peer();

	int getProgress(); 							//return a number with represent the percentage number of the filled bit_piece amounts

	int initStartTime();						//get the start time stamp and save it to start_time

	double getCurrentTime();					//return a time in second format count from the initialize time

	void updatePeerConnectionStatus(int timeout); //handle the situation that the client closed unexpected, update every status

	void updatePeerName(int peer_num);          //update a peer's name information, the name is represented by ip:port, in a readable format

	void updateLocalName();						//update local server's name information, in readble format ip:port

	void readSock(int sock, void *buffer, int read_num, int *sum); //read from a socket for a certain number of 'read_num' if there exists, write the actual read number to sum

	void standardOutput();							// the standard output when not in verbose mode

	int updateLocalInfo(char *host_name, unsigned short port); //update local addr info, return complete status

	int updatePeerInfo(char *host_name, unsigned short port, int peer_num); //update peer addr info, return complete status

	int sendSock(int dest_sock, void *buffer, int send_num, int &act_send_num); //send data to socket, return status

	int checkHash(void *begin, unsigned int len); //check data(start from 'begin' with length 'len')'s data integrity, if correct return 0, else return 1

	int attachHash(const void *data_begin, unsigned int data_len, void *hash_begin); //use the piece of data(from 'data_begin' of length 'data_len'), generate the hash value and attach it to the destination

	int getTorrentMap(); //use torrent fn to generate a map which stores torrent info

	int getTorrentInfoHash(); //get torrent info's hash value and write to 'info_hash', run after 'getTorrentMap', the info contains four components: 'length', 'name', 'piece length', 'pieces', this hash will be created on these four values

	int calcLocalId(); //calculate local id using local_addr information(port and address)

	int calcPeerId(int peed_num); //calculate peer id using input information

	int msgInterpret(char *buffer); //interpret the meg stored in buffer, return the msg type, if failed, return -1

	int msgHandler(char *buffer, unsigned int len, int peer_num); //react based on local peer's type

	int initSaveFileWritePointer(); //initialize the file write pointer to save file which can last through the peer's live period

	int initLogFileWritePointer(); //initialize the file write pointer to log file which can last through the peer's live period

	int initSourceFileReader();   //use FILE to open a source file "rb"

	int initLogFileWriter();       //use FILE to open a log file "w+""

	int readSourceFile(void *begin, int offset, int len);

	int writeToLogFile(const char*); //require initLogFileWritePointer, write to log file

	int writeToSaveFile(void *buffer, int offset, long unsigned int len); //requre initSaveFileWritePointer, write to save file

	int closeSaveFileWritePointer(); //close the save file write pointer

	int closeLogFileWritePointer(); //close log file write pointer

	std::string getMsgType(int type);

	connection_struc peer_connection[MAX_CONNECTIONS];

	int printTorrentMap(); //print of the torrent_map

	int pollPeerIncome(int timeout); //poll on peer income for seeder or leecher to accept a new connection(timeout 0), return -1 if no active peer, return 0 if exit normally and no socket is ready, else return a socket number

	int initLocalPeerIncome(); //initialize the local income and peer income structure

	int initPeerConnection(); //initialize peer connection table as non-'exist' and non-'active' and 'choke' and non-'interested'

	int sendHandShakeMsg(int sock); //send handshake msg through sock, return -1 if failed

	int initPieceBitNumber(); //initialize piece number and bit number, piece number refer to torrent_map and bit number set to 0

	int incomeHandler(int peer_num); //handle income from socket, return negative number if encouter an unkonw income

	int getNextAvailPeerNum(); //get the a available peer_num, when the peer_connection[] member is not 'exist', return peer_num if found, else return -1

	int initPeerLocalIncomePoll(); //init all local and peer incoming poll structure, set 'fd' field to -1

	int addPeerIncomePoll(int sock); //add sock to peer incoming poll, return -1 if fail, 0 if success

	int addLocalIncomePoll(int sock); //add sock to local incoming poll, return -1 if fail, 0 if success

	int getPeerNumFromSock(int sock); //search peer_connection table to find peer_num with sock, if not found return -1, else return peer_num

	void printPeer(); //print the list of peer in the peer_connection table which 'exist'

	void setPeerType(int type); //set the PeerType to given value;

	void printPeerConnection(); //print peer connection table for test

	void peerClose(int peer_num); //close peer and file descriptor



	/* for seeder */
	int initBindServerSock(); //create a socket and bind to it, only for seeder; if success return complete status, write socket number to server_sock, return 1 if socket() fail, return 2 if bind() fail

	int listenServerSock(); //listen on server_sock and add it to local_income, return -1 if listen fail, return -2 if local_income adding fail, return 0 if success

	int acceptClntSock(); //accept a connection from the socket, put it into the peer_connection[index], return -1 if accept failed, return -2 if peer_connection[] is full

	//int assemChokeMsg(char *begin, int &len); //assemble a choke message to 'begin', expected to have 5 bytes

	int assemUnChokeMsg(char *begin, int &len); //assemble an unchoked message to 'begin', expected to have 5 bytes

	int assemBitFieldMsg(char *begin, int &len); //assemble a bitField Msg to 'begin', expected to have 5 + bitfield length

	int assemPieceMsg(char *begin, void *buffer, unsigned int write_len, unsigned int index, unsigned int offset, int &len); //assemble a Piece Msg, get data from buffer for length write_len, the piece itself is from 'index' number of pieces and start from offset number

	int assemSeederHandShake(); //assemble handshake message using local id for seeder, put it to private member 'hand_shake', publicly used, return complete status

	int pollLocalIncome(int timeout); //poll on local income for seeder to accept new connections(timeout 0), return 0 if exit normally and no sock is ready, else return a socket number

	int sendUnChokeMsg(int sock); //send unchoke msg through sock, only for seeder

	int sendBitField(int sock); //send bitfield msg through sock, only for seeder

	int sendPieceMsgs(int sock, unsigned index, unsigned int offset, unsigned int piece_len); //send piece msg through sock, only for seeder



	/* for leecher */
	int updateDestInfo(char *host_name, int index); //return complete status

	int setBitField(int index, int value); //set BitField(start from start_index for 'len' bit length) using the value given

	bool getBitValue(int index); //get the bit value of index

	bool getBitContactValue(int index); //get the bit value of bit contacted index

	int initBitFieldWithZero(); //initialize bitfield all to 0

	int initBitFieldWithOne(); //initialize bitfield all to 1

	int initBitFieldContactWithZero();

	int setBitFieldContact(int index, int value);

	int assemInterestMsg(char *begin, int &len); //assemble a interested message to 'begin', expected to have 5 bytes

	//int assemNotInterestMsg(char *begin, int &len); //assemble a not-interested message to 'begin', expected to have 5 bytes

	int assemHaveMsg(unsigned int index, char *begin, int &len); //assemble a have msg, which contains index of peice just complete, expected to have 9 bytes

	int assemRequestMsg(char *msg_begin, unsigned int index, unsigned int begin, unsigned int length, int &len); //assemble a Request msg, which contains three unsigned int type

	//int assemCancelMsg(char *msg_begin, unsigned int index, unsigned int begin, unsigned int length, int &len); //assemble a cancel msg, which contains the same layout as request

	int assemLeecherHandShake(int peer_num); //assemble handshake message using peer id for leecher, put it to 'hand_shake', which is publicly used, return complete status

	int sendInterestMsg(int sock); //send interested message through sock, only for leecher

	int sendHaveMsg(int sock, unsigned int index); //send have message through sock, only for leecher

	int sendRequestMsg(int sock, unsigned int index, unsigned int offset, unsigned int data_len); //send request message through sock, only for leecher

	int initSourceFile(); //get source file name from torrent_map

	int getNextIndex(); //get the next index of pieces to request, return -1 if no index needed, return -2 if failed, else return the index

	int checkFileHash(); //check the file hash value with the hash value given in torrent file, if match return 1, if not match end normal return 0, set the not match parts bitfield back to 0, if failed return -1

	int connectToPeer(); //check the connection table to find 'exist'(if not find, return -1) but not 'active', try to socket(fail:1) and connect(fail:2), send handshake(fail:-2), add to peer_income(fail:-3) and add to peer_income(fail 3); return 0 on success

};

#endif /* PEER_H_ */
