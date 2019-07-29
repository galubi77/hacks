# hacks
compilation lines :
gcc -s -static -o xping ping.c
gcc -s -static -o xtraceroute traceroute.c
gcc -s -static -o xsmurf smurf.c
gcc -s -static -o xupstorm upstorm.c

note need to set uid and run with sudo because of the raw sockets
