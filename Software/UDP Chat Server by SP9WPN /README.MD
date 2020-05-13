# Morserino Chat Server by Wojtek, SP9WPN

Wojtek has created a Python script that you can put on an Internet facing server. Up to ten Morserinos can connect to it through WiFi Trx, and so can have Morse Code conversations across the Internet.

From Wojtek's description:

>This server script listens for UDP messages and rebroadcasts them to all other clients.
>
>To "connect" or "make yourself known" to the server, send "hi" message from your >Morserino. Server will respond with ":hi". After that, server will resend all messages >from you to all other known clients (you excluded).
>
>If client is inactive (not transmitting) for too long, it will be dropped from the list. >This is indicated by ":bye" message sent from the server. To reconnect, send "hi" again.

You can find the script and documentation here: <https://github.com/sp9wpn/m32_chat_server>.
