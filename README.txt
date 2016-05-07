Jianqi Zhao (zhao61)
Yuxuan Chai (yuchai)
——————————

This project implement the n to n bitTorrent client, it can realize real-time n to n file transfer. 

For this project, you will find the following files:
1.source folder, including
‘bt_client.cpp’
‘Helper.cpp’
‘Helper.h’
‘Peer.cpp’
‘peer.h’
‘torrent.cpp’
‘torrent.h’
2.”makefile”
3.log samples(including leecher’s log and seeder’s log)
4.two-two.sh (this file at least shows how to run the program though it’s not permitted to run on silo)
5.sample torrent file and source file(the files given to us)


To run this program, you need to (1)compile it using makefile (2)either run it by “./Bittorrent“ and look at the instructions or you can look at the two-two.sh file which provides the commands to start a two-to-two seeder-leecher environment

note that: the leecher and seeder executable file are all the same one(‘Bittorrent’), the one with a “-b” argument is a seeder and with a ‘-p’ argument is the leecher. Also note the torrent file and source file need to be the same folder as the executable file to let it access them

Now, I simply describe the functions of each code file:

‘bt_client’: this is the main file, which starts to run the program

‘Helper.cpp’ and ‘Helper.h’ provide some trivial functions like file input to facilitate the the bitTorrent function

‘peer.cpp’ and ‘peer.h’ is where the class of the bitTorrent object lies in, it contains a lot of Bittorrent functions which almost consider all of the behaviors it can have

‘torrent.cpp’ and ‘torrent.h’ gives a simple function to parse the torrent file, which is the goal of the milestone 1.  