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

## How to use

Download the library from GitHub, and unzip it into your *libraries* folder inside your Arduino *sketches* folder.

---

Include the library in your sketch:

    #include <HTTPserver.h>

---

Derive an instance of the *HTTPserver* class with custom handlers (just the ones you want):

    class myServerClass : public HTTPserver
      {
      virtual void processPostType        (const char * key, const byte flags);
      virtual void processPathname        (const char * key, const byte flags);
      virtual void processHttpVersion     (const char * key, const byte flags);
      virtual void processGetArgument     (const char * key, const char * value, const byte flags);
      virtual void processHeaderArgument  (const char * key, const char * value, const byte flags);
      virtual void processCookie          (const char * key, const char * value, const byte flags);
      virtual void processPostArgument    (const char * key, const char * value, const byte flags);
      };  // end of myServerClass

---

Make an instance of this new class:

    myServerClass myServer;

---

Now implement the handlers that you declared above, most have key/value arguments. For example:

    void myServerClass::processGetArgument (const char * key, const char * value, const byte flags)
      {
      if (strcmp (key, "light") == 0 && strcmp (value, "on") == 0)
        digitalWrite (light_switch, HIGH);
      }  // end of myServerClass::processGetArgument

---

When you get an incoming connection call the begin() method to reset the state machine to the start:

    myServer.begin (&client);


In the begin() call we pass down the address of the Ethernet client, so that the derived classes can do print() and println() calls to send data back to the web client. If this isn't possible pass down NULL.

---

Now while the client remains connected, call *processIncomingByte* for each byte read from the web client, this will be processed by the state machine, and the appropriate callback routine (which you supplied earlier) will be called when required. The state machine sets the *done* flag when it determines that there should be no more incoming data.

    while (client.connected() && !myServer.done)
      {
      while (client.available () > 0 && !myServer.done)
        myServer.processIncomingByte (client.read ());

      // do other stuff here

      }  // end of while client connected

---

## Output buffering

Version 1.2 of this library now buffers output. This considerably speeds up writes done with the F() macro, eg.

    void myServerClass::processCookie (const char * key, const char * value, const byte flags)
      {
      print (F("Cookie: "));
      print (key);
      print (F(" = "));
      println (value);
      }  // end of processCookie

Previously each byte in those strings would be sent in a separate packet, now they are placed into a 64-byte buffer, and sent when the buffer fills up. When you are done sending you need to flush the final bytes like this:

      myServer.flush ();

This buffering is only done if you write **via the derived class**, not directly to the client. For example:

      myServer.println(F("<html>"));
      myServer.println(F("<body>"));
