# Dummy Hermes 

This project has been inspired by [Smallchat](https://github.com/antirez/smallchat) of Salvatore Sanfilippo aka antirez. I was struck by his statement:
> writing a very simple IRC server is an experience everybody should do

I mean that guy is the creator of Redis, it shouldn't be a bad idea to follow one of his advices.

## Current work and future features:

Nowadays the chat only provides extremely basic features. I'd like to implement a few new ones:
- set a username/nickname for each client
- improve the editing of messages for a client: now when a user is typing a message it may receive messages from other clients and this stuff concatenates to what he's currently written, eww 😖
- modularize the project, for example network logic could be placed in a separated file
