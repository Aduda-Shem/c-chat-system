# compiling server
------------------------------------

gcc -o server server.c -lz

# compiling client
-------------------------------------

gcc -o client client.c



# Running
----------------------------------- 
server
     ./server 12345

Registeration
./client 127.0.0.1 12345 admin admin registration

Login
./client 127.0.0.1 12345 admin admin login

send message
./client 127.0.0.1 12345 admin admin "It was not Easy"
