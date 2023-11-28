# Dummy Hermes 

This project has been inspired by [Smallchat](https://github.com/antirez/smallchat) of Salvatore Sanfilippo aka antirez. I was struck by his statement:
> writing a very simple IRC server is an experience everybody should do

I mean that guy is the creator of Redis, it shouldn't be a bad idea to follow one of his advices.

## Current work and future features:

Nowadays the chat only provides extremely basic features. I'd like to implement a few new ones:
- add more commands for clients beside `\setusername`
- add control and validation for the editing of messages for a client: a client can move the cursor in any position when editing the text message, this is not so nice
- modularize the project, for example network logic could be placed in a separated file
