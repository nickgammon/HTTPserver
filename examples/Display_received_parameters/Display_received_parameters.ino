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
  virtual void processPathname        (const char * key, const byte flags);
  virtual void processHttpVersion     (const char * key, const byte flags);
  virtual void processGetArgument     (const char * key, const char * value, const byte flags);
  virtual void processHeaderArgument  (const char * key, const char * value, const byte flags);
  virtual void processCookie          (const char * key, const char * value, const byte flags);
  virtual void processPostArgument    (const char * key, const char * value, const byte flags);
  };  // end of myServerClass

myServerClass myServer;

// -----------------------------------------------
//  User handlers
// -----------------------------------------------

void myServerClass::processPostType (const char * key, const byte flags)
  {
  println(F("HTTP/1.1 200 OK"));
  println(F("Content-Type: text/plain\n"
            "Connection: close\n"
            "Server: HTTPserver/1.0.0 (Arduino)"));
  println();  // end of headers

  print (F("GET/POST type: "));
  println (key);
  } // end of processPostType

void myServerClass::processPathname (const char * key, const byte flags)
  {
  print (F("Pathname: "));
  println (key);
  }  // end of processPathname

void myServerClass::processHttpVersion (const char * key, const byte flags)
  {
  print (F("HTTP version: "));
  println (key);
  }  // end of processHttpVersion

void myServerClass::processGetArgument (const char * key, const char * value, const byte flags)
  {
  print (F("Get argument: "));
  print (key);
  print (F(" = "));
  println (value);
  }  // end of processGetArgument

void myServerClass::processHeaderArgument (const char * key, const char * value, const byte flags)
  {
  print (F("Header argument: "));
  print (key);
  print (F(" = "));
  println (value);
  }  // end of processHeaderArgument

void myServerClass::processCookie (const char * key, const char * value, const byte flags)
  {
  print (F("Cookie: "));
  print (key);
  print (F(" = "));
  println (value);
  }  // end of processCookie

void myServerClass::processPostArgument (const char * key, const char * value, const byte flags)
  {
  print (F("Post argument: "));
  print (key);
  print (F(" = "));
  println (value);
  }  // end of processPostArgument

// -----------------------------------------------
//  End of user handlers
// -----------------------------------------------

void setup ()
  {
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, gateway, subnet);
  }  // end of setup

void loop ()
  {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (!client)
    return;

  myServer.begin (&client);
  while (client.connected() && !myServer.done)
    {
    while (client.available () > 0 && !myServer.done)
      myServer.processIncomingByte (client.read ());

    // do other stuff here

    }  // end of while client connected

  myServer.println(F("OK, done."));
  myServer.flush ();

  // give the web browser time to receive the data
  delay(1);
  // close the connection:
  client.stop();

  }  // end of loop

