Twitter Server built in C: final assignment for Software Tools and Systems Programming at U of T

Open a new terminal and use the following command: "Make PORT=x", where x is the number of an open port. E.g. 53457. This will compile the server's code to run on the specified port. If running more than once, you may have to change the port as it may not be free after running. To start server run command: "./twerver"

Open a second terminal and and use the following command: "nc -C localhost x", where is the is number of the open port. You will be promted to enter a username. Multiple users can connect simultaneously by repeating this with new terminals.

The program can also communicate over different machines. See https://linux.die.net/man/1/nc for more details on connecting through different machines with netcat

The following a list of commands:
- "follow x" : x is the username to follow
- "unfollow x" : x is the username to unfollow
- "show" : displays all previously sent messages of users the user is follow
- "send x" : x is a message that is up to 140 characters long
- "quit" : closes and terminates the socket of a user
