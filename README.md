# CRUx R3 Submission

My R3 consisted of 2 tasks

## Task 1 (Improve Chat App) (Current Repo)

- Improved on my existing chat application and rewrote it's framework to support Event Based message transfers
- Added logging to 2 write targets, stdout and JSON
- Implemented JWT based username authentication.

## Task 2 (Dungeon Game Dev)

- Created a 2D, top-down dungeon crawler game.
- Implemented traps and powerups
- Implemented an FOV system.

Repo Link: <a href = "https://github.com/hans772/KnightOfTheDungeon?tab=readme-ov-file#dungeoncrawler"> Dungeon Crawler </a>


# winMessenger

This is a very bare bones messaging app I made, It uses raw sockets to send and recieve messages through a server to other clients

## Usage

Usage is very straightforward, Run the program.<br>

### Creating a server

To start a server (cannot send messages but is necessary to host, the user has 2 options:

#### Threaded Server

Create a threaded server, each client will be handled by a separate thread.

#### Event Based Server

Create an event based server, the server creates a fixed number of threads on startup. every read and write operation is done 
asynchronously in a non blocking manner. the worker threads will recieve a context to handle what to do after each operation. This is
more efficient for larger servers.<br><br>

Choose the port you want the server to be hosted on and the maximum clients you want to allow to connect.
Voila! you have a server.<br>

### Creating a client

To start a client (used to send messages to and recieve messages from a server) enter the IP and PORT the server is hosted on.<br>
Choose which chatroom you wish to join and your desired name (Only first time you connect, connecting to the same server after that will only require your room)<br>

A client can only text people in the same Chatroom as them<br>

A client can choose to join a Subroom in their respective Chatroom, where the same rules apply.

#### Commands

Some commands have been added, a command can be run by prefixing a message with '/'
- Subroom: usage: /subroom <join/list> <subroom-name>    Used to create/list subrooms.
- Exit: usage: /exit    Used to exit the server.

### Menu Usage

The menu is controlled with the arrow keys (up and down), to enter a custom input, check each section for the "custom*" option.
Select that option, an entry will be created to input your custom value;

## Footnotes

Since the server is being hosted on a local machine, port forwarding must be enabled for the server to be visible to devices outside the LAN.<br>
However any device inside the LAN will be able to connect to your device using the device specific IP address assigned to the device by the LAN.<br>

## Credits

I have used an nlohmann:json header file to serialize and deserialize headers in json format
Here is a link to the repository: https://github.com/nlohmann/json

## Plans

I have plans on adding an image sharing / file sharing mode.
