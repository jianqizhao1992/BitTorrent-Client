#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h> //ip hdeader library (must come before ip_icmp.h)
#include <netinet/ip_icmp.h> //icmp header
#include <arpa/inet.h> //internet address library
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#include "Peer.h"
#include "Helper.h"

int global_x = 0;                  //use a global variable to help shuffle the available connections

int main (int argc, char * argv[]){

  Peer local_peer;
  local_peer.verbose = 0;
  local_peer.log = 0;
  local_peer.setPeerType(1); //the peer will be stay as leecher unless it specify the '-b' value

  //initialize peer and local's income poll
  local_peer.initPeerLocalIncomePoll();

  //initial the peer_connection table as non-'exist' and non-'active' and 'choke' and non-'interested'
  local_peer.initPeerConnection();



  Helper::parse_args(local_peer, argc, argv); 	//For leecher: the initial bunch of destination addr and id have been initialized here and set as 'exist'
  	  	  	  	  	  	  	  	  	  	  	  	//For seeder: the local addr and id info has been initialized


  //read and parse the torrent file
  local_peer.getTorrentMap();
  local_peer.getTorrentInfoHash();
  local_peer.initSourceFile();
  local_peer.initSourceFileReader();
  local_peer.initLogFileWriter();

  //initialize file descriptor for write
  local_peer.initSaveFileWritePointer();

  //initialize piece number and bit field pointer
  local_peer.initPieceBitNumber();

  //initialize peer and local's income poll
  local_peer.initPeerLocalIncomePoll();

  if(local_peer.verbose){
	  //print out the torrent file arguments here
	  if(local_peer.type == 0)
		  std::cout << "source file: " << local_peer.source_file << std::endl;
	  else
		  std::cout << "save file: " << local_peer.save_file << std::endl;
	  if(local_peer.log)
		  std::cout << "log file: " << local_peer.log_file << std::endl;
	  std::cout << "torrent file: " << local_peer.torrent_file << std::endl;
	  local_peer.printTorrentMap();
  }

  //main client loop
  if(local_peer.type == 0) 				 //proceed as a seeder
  {
	  std::cout << "start as a seeder" << std::endl;
	  int cur_peer;
	  //initialize bitfield information
	  local_peer.initBitFieldWithOne();
	  //generate a server side handshake
	  local_peer.assemSeederHandShake();
	  //init a local socket, bind it
	  local_peer.initBindServerSock();
	  //listen to the local socket, add the local socket to "local incoming poll" structure
	  local_peer.listenServerSock();

	  while(++global_x){
		  //poll "local incoming poll" for incoming connection

		  if((local_peer.pollLocalIncome(100)) > 0){
		  	  //accept incoming connection from new peer, fill connection table, set 'active to 1', set 'exist' to 1, add accepted sock to "peer incoming poll" structure
			   local_peer.acceptClntSock();
		  }

		  //std::cout << "global_x" << global_x << std::endl;
		  if((cur_peer = local_peer.pollPeerIncome(100)) >= 0){

			  local_peer.incomeHandler(cur_peer);
		  }

		  local_peer.updatePeerConnectionStatus(0);
		  if(global_x % 5 == 0){
			  local_peer.standardOutput();
		  }
	  }
  }
  else if(local_peer.type == 1)          //proceed as a leecher
  {
	  std::cout << "start as a leecher" << std::endl;
	  int cur_peer;
	  //initialize bit field
	  local_peer.initBitFieldWithZero();
	  local_peer.initBitFieldContactWithZero();

	  while(++global_x){

		  if((cur_peer = local_peer.pollPeerIncome(100)) > 0){

			  local_peer.incomeHandler(cur_peer);
		  }

		  local_peer.connectToPeer();
		  if(global_x % 5 == 0){
			  local_peer.standardOutput();
		  }
	  }
  }
  return 0;
}
