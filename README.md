# Tiny web server for Arduino or similar

This is the code and example sketches for a library which interprets incoming HTTP messages using a "state machine":

* Minimal memory (RAM) requirements (about 256 bytes)
* Small code size (around 2.5 KB)
* No use of the String class, or dynamic memory allocation (to avoid heap fragmentation)
* Incoming HTTP (client) requests decoded "on the fly" by a state machine
* Doesn't care what Ethernet library you are using - you call your Ethernet library, and send a byte at at time to the HTTP library.
* Compact and fast
* Handles all of:

    *    Request type (eg. POST, GET, etc.)
    *    Path (eg. /server/foo.htm)
    *    GET parameters (eg. /server/foo.htm?device=clock&mode=UTC)
    *    Header values (eg. Accept-Language: en, mi)
    *    Cookies (as sent by the web browser)
    *    POST data (ie. the contents of forms)

See: [Forum posting with description](http://www.gammon.com.au/forum/?id=12942)
