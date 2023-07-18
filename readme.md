this file is to explain the usages, so to un the chat tool you should run 'make all' and then "./stnc -1 -s PORT" for server and "./stnc -1 -c IP PORT" for client.
for the usage of the perfomance test "./stnc -2 -s PORT -p -q" for the server and "./stnc -2 -c IP PORT -p <type> <param>" for the client.
one minor problem we faced is that some communication styles didnt work as expected when unning the main sever and therefore you will need to run each server and its client and comment out the rest of the server, the poblem is in the server and the main client is working fine.
