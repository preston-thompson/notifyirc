# notifyirc
Small IRC bot written in C to notify users on input from stdin.

## Usage

The bot will connect to the IRC server configured in `notifyirc.c` and wait for a line on stdin. Upon receiving one, it will query the server for a list of names in a channel configured in `notifyirc.c`. Then the bot will privately message each of the users with the text from stdin. Then the bot will wait for the next line on stdin.

The command line arguments are:

```
usage: ./notifyirc nick channel
```

Where `nick` is the nick of the bot, and `channel` is the channel containing the people to be messaged.

## Bugs

* Doesn't handle disconnects.
* Hangs when there's nobody in the channel (channel does not exist).
