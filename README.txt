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
client
    ./client localhost 123456 2 < intext.txt

    ./client localhost 12345 1 < intext.txt

# Generate .py
the above folder should be used to populate data in the intext.py
    python generate.p