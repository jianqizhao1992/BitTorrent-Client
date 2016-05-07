#!/bin/bash
./Bittorrent -v -b localhost:4848 -l seeder1.log download.mp3.torrent
./Bittorrent -v -b localhost:4949 -l seeder2.log download.mp3.torrent
./Bittorrent -v -p localhost:4848 -p localhost:4949 -s save_file_l1.mp3 -l leecher1.log download.mp3.torrent
./Bittorrent -v -p localhost:4848 -p localhost:4949 -s save_file_l2.mp3 -l leecher2.log download.mp3.torrent
