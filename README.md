# winMessenger

This is a very bare bones messaging app I made, It uses raw sockets to send and recieve messages through a server to other clients

## Usage

Some commands have been added, a command can be run by prefixing a message with '/'
- Subroom: usage: `/subroom join <subroom-name>`.
- Subroom: usage: `/subroom create <subroom-name>`.
- Subroom: usage: `/subroom parent`.

## Info

Since the server is being hosted on a local machine, port forwarding must be enabled for the server to be visible to devices outside the LAN.<br>
However any device inside the LAN will be able to connect to your device using the device specific IP address assigned to the device by the LAN.<br>

## Credits

I have used an nlohmann:json header file to serialize and deserialize headers in json format
Here is a link to the repository: https://github.com/nlohmann/json

## Plans

I have plans on adding an image sharing / file sharing mode.
