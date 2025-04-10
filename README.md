# winMessenger

This is a very bare bones messaging app I made, It uses raw sockets to send and recieve messages through a server to other clients

## Usage

Usage is very straightforward, Run the program.<br>
-To start a server (cannot send messages but is necessary to host) enter 0 and then enter port and the number of clients you want to accept. Voila! you have a server.<br>
-To start a client (used to send messages to and recieve messages from a server) enter 0, and then the IP and PORT the server is hosted on. Enter your name and enjoy texting.<br>

Since the server is being hosted on a local machine, port forwarding must be enabled for the server to be visible to devices outside the LAN.<br>
However any device inside the LAN will be able to connect to your device using the device specific IP address assigned to the device by the LAN.<br>

If you do not want to use visual studio to compile the code, I have added an msi installer into a zip file, which you can use to test out the program<br>

# Credits

I have used an nlohmann:json header file to serialize and deserialize headers in json format
Here is a link to the repository: https://github.com/nlohmann/json

## Plans

I have plans on adding an image sharing / file sharing mode.
