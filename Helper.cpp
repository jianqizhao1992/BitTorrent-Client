#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Helper.h"

void Helper::DieWithSystemMessage(const char *msg){
	perror(msg);
	exit(1);
}

void Helper::DieWithUserMessage(const char *msg, const char *detail){
	fputs(msg, stderr);
	fputs(": ", stderr);
	fputs(detail, stderr);
	fputc('\n', stderr);
	exit(1);
}

void Helper::LiveWithSystemMessage(const char *msg){
    perror(msg);
}

void Helper::LiveWithUserMessage(const char *msg, const char *detail){
	fputs(msg, stderr);
	fputs(": ", stderr);
	fputs(detail, stderr);
	fputc('\n', stderr);
}

int Helper::writeFile(std::fstream *fs, void *buffer, int offset, const char *filename, long unsigned int len){
	fs->seekp(offset*sizeof(char), std::ios::beg);
	fs->write((const char *)buffer, len);
	return 0;
}

int Helper::readFile(void *begin, char *fn, int offset, int len){
	FILE *f = fopen(fn, "rb");
	if(f == NULL)
		Helper::DieWithSystemMessage("File open error");
	fseek(f, offset, SEEK_SET);
	int result = fread(begin, sizeof(char), len, f);
	if(result != len){
		LiveWithSystemMessage("File read inconsistent number data");
		return -1;
	}
	fclose(f);
	return 0;
}

void Helper::__parse_peer(Peer &client_x, int peer_num, char * peer_st){
	char * parse_str;
	char * word;
	unsigned short port;
	char * ip;
	char sep[] = ":";
	int i;

	parse_str = (char*)malloc(strlen(peer_st)+1);
	strncpy(parse_str, peer_st, strlen(peer_st)+1);

	//only can have 2 tokens max, but may have less
	for(word = strtok(parse_str, sep), i=0; (word && i < 3); word = strtok(NULL,sep), i++){
		switch(i){
		case 0://id
			ip = word;
			break;
		case 1://ip
			port = atoi(word);
		default:
			break;
		}
	}

	if(i < 2){
		fprintf(stderr,"ERROR: Parsing Peer: Not enough values in '%s'\n",peer_st);
		Helper::usage(stderr);
		exit(1);
	}

	if(word){
		fprintf(stderr, "ERROR: Parsing Peer: Too many values in '%s'\n",peer_st);
		Helper::usage(stderr);
		exit(1);
	}

	client_x.updatePeerInfo(ip, port, peer_num);
	client_x.updatePeerName(peer_num);
	client_x.calcPeerId(peer_num);

	//free extra memory
	free(parse_str);

	return;
}

void Helper::__parse_local(Peer &client_x, char * peer_st){
	  char * parse_str;
	  char * word;
	  unsigned short port;
	  char * ip;
	  char sep[] = ":";
	  int i;

	  parse_str = (char*)malloc(strlen(peer_st)+1);
	  strncpy(parse_str, peer_st, strlen(peer_st)+1);

	  //only can have 2 tokens max, but may have less
	  for(word = strtok(parse_str, sep), i=0; (word && i < 3); word = strtok(NULL,sep), i++){
	      switch(i){
	      	  case 0://id
	      		  ip = word;
	      		  break;
	      	  case 1://ip
	      		  port = atoi(word);
	      	  default:
	      		  break;
	      }
	  }
	  if(i < 2){
		  fprintf(stderr,"ERROR: Parsing Bind: Not enough values in '%s'\n",peer_st);
		  Helper::usage(stderr);
		  exit(1);
	  }
	  if(word){
		  fprintf(stderr, "ERROR: Parsing Bind: Too many values in '%s'\n",peer_st);
		  Helper::usage(stderr);
		  exit(1);
	  }
	  client_x.updateLocalInfo(ip, port);
	  client_x.calcLocalId();
	  client_x.updateLocalName();
	  std::cout << "local ip:port " << client_x.local_name << std::endl;
	  free(parse_str);
	  return;
}

void Helper::parse_args(Peer &client_x, int argc,  char * argv[]){
	int ch; //ch for each flag
	int n_peers = 0;
	int i;

	/* set the default args */
	client_x.verbose = 0;

	memset(client_x.torrent_file, 0x00, FILE_NAME_MAX);
	memset(client_x.save_file, 0x00, FILE_NAME_MAX);
	memset(client_x.log_file, 0x00, FILE_NAME_MAX);
	memset(client_x.source_file, 0x00, FILE_NAME_MAX);

	//default log file
	strncpy(client_x.log_file, "bt_client.log", FILE_NAME_MAX);

	for(i=0;i<MAX_CONNECTIONS;i++){
		client_x.peer_connection[i].active = 0; //initially NULL
	}

	while ((ch = getopt(argc, argv, "vhp:s:l:b:")) != -1) {
		switch (ch) {
			case 'h': //help
				usage(stdout);
				exit(0);
				break;
			case 'v': //verbose
				client_x.verbose = 1;
				break;
			case 's': //save file
				strncpy(client_x.save_file,optarg,FILE_NAME_MAX);
				break;
			case 'l': //log file
				client_x.log = 1;
				strncpy(client_x.log_file,optarg,FILE_NAME_MAX);
				break;
			case 'b': //local bind
				client_x.setPeerType(0);                //set peer as a seeder
				__parse_local(client_x, optarg);
				break;
			case 'p': //peer
				n_peers++;
				//check if we are going to overflow
				if(n_peers > MAX_CONNECTIONS){
					fprintf(stderr,"ERROR: Can only support %d initial peers",MAX_CONNECTIONS);
					usage(stderr);
					exit(1);
				}
				//parse peers
				__parse_peer(client_x, n_peers, optarg);
				break;
			default:
				fprintf(stderr,"ERROR: Unknown option '-%c'\n",ch);
				usage(stdout);
				exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	if(argc == 0){
		fprintf(stderr,"ERROR: Require torrent file\n");
		usage(stderr);
		exit(1);
	}
	else{
		strncpy(client_x.torrent_file, *argv, FILE_NAME_MAX);
	}
	return ;
}

void Helper::usage(FILE * file){
	if(file == NULL){
		file = stdout;
	}

	fprintf(file,
			"bt-client [OPTIONS] file.torrent\n"
			"  -h            \t Print this help screen\n"
			"  -b ip:port    \t Bind to this ip and port for incoming connections\n"
			"  -s save_file  \t Save the torrent in directory save_dir (dflt: .)\n"
			"  -l log_file   \t Save logs to log_filw (dflt: bt-client.log)\n"
			"  -p ip:port    \t Instead of contacing the tracker for a peer list,\n"
			"                \t use this peer instead, ip:port (ip or hostname)\n"
			"                \t (include multiple -p for more than 1 peer)\n"
			//"  -I id         \t Set the node identifier to id (dflt: random)\n"
			"  -v            \t verbose, print additional verbose info\n");
}

