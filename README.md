# Minesweeper

This repository harbours the project for the application of multithreading in C with a Minesweeper Server allowing 10 clients to connect up and play with a live leaderboard.

Navigate to the directory the file is in and compile using: make   

Run the server first (12345 is the port number and can be replaced by a number of your choice): 
./Server 12345   

Run up to 10 clients using: 
./Client localhost 12345 (12345 is the port number and this has to be the same as the server to run) 