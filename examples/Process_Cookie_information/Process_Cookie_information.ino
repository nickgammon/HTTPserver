// Tiny web server demo
// Author: Nick Gammon
// Date:  20 July 2015

#include <SPI.h>
#include <Ethernet.h>
#include <HTTPserver.h>

// Enter a MAC address and IP address for your controller below.
byte mac[] = {  0x90, 0xA2, 0xDA, 0x00, 0x2D, 0xA1 };

// The IP address will be dependent on your local network:
byte ip[] = { 10, 0, 0, 241 };

// the router's gateway address:
byte gateway[] = { 10, 0, 0, 1 };

// the subnet mask
byte subnet[] = { 255, 255, 255, 0 };

// Initialize the Ethernet server library
EthernetServer server(80);

// derive an instance of the HTTPserver class with custom handlers
class myServerClass : public HTTPserver
  {
  virtual void processPostType        (const char * key, const byte flags);
  virtual void processCookie          (const char * key, const char * value, const byte flags);
  virtual void processPostArgument    (const char * key, const char * value, const byte flags);

  public:

  char theirName [30];

  };  // end of myServerClass

myServerClass myServer;

// -----------------------------------------------
//  User handlers
// -----------------------------------------------


void myServerClass::processPostType (const char * key, const byte flags)
  {
  println(F("HTTP/1.1 200 OK"));
  println(F("Content-Type: text/html"));
  theirName [0] = 0;  // no name yet
  } // end of processPostType


void myServerClass::processCookie (const char * key, const char * value, const byte flags)
  {
  // if we get a "name" cookie, save the name
  if (strcmp (key, "name") == 0)
    {
    strncpy (theirName, value, sizeof (theirName) - 1);
    theirName [sizeof (theirName) - 1] = 0;
    }
  }  // end of processCookie

void myServerClass::processPostArgument (const char * key, const char * value, const byte flags)
  {
  // if they sent a form with a new name, set the cookie to it
  if (strcmp (key, "name") == 0)
    {
    strncpy (theirName, value, sizeof (theirName) - 1);
    theirName [sizeof (theirName) - 1] = 0;
    setCookie ("name", value);
    }
  } // end of myServerClass::processPostArgument

// -----------------------------------------------
//  End of user handlers
// -----------------------------------------------

void setup ()
  {
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
  }  // end of setup

void loop ()
  {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (!client)
    {
    // do other processing here
    return;
    }

  myServer.begin (&client);
  while (client.connected() && !myServer.done)
    {
    while (client.available () > 0 && !myServer.done)
      myServer.processIncomingByte (client.read ());

    // do other stuff here

    }  // end of while client connected

  // end of headers
  client.println();
  // start HTML stuff
  client.println (F("<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Arduino test</title>\n"
             "</head>\n"
             "<body>\n"));

  client.print (F("<p>Your name is registered as: "));
  myServer.fixHTML (myServer.theirName);
  client.println ();

  client.println (F("<p><form METHOD=\"post\" ACTION=\"/change_name\">"));
  client.print   (F("<p>Your name:&nbsp;<input type=text Name=\"name\" size="));
  client.print   (sizeof myServer.theirName - 1);
  client.print   (F(" maxlength="));
  client.print   (sizeof myServer.theirName - 1);
  client.print   (F(" value=\""));
  client.print   (myServer.theirName);
  client.println (F("\">"));
  client.println (F("<p><input Type=submit Name=Submit Value=\"Save\">"));
  client.println (F("</form>"));

  client.println(F("<hr>OK, done."));

  client.println (F("</body>\n"
                    "</html>"));


  // give the web browser time to receive the data
  delay(1);
  // close the connection:
  client.stop();

  }  // end of loop
