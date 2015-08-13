// Tiny web server demo
// Author: Nick Gammon
// Date:  20 July 2015

#include <SPI.h>
#include <Ethernet.h>
#include <HTTPserver.h>

const int LOW_PIN = 3;
const int HIGH_PIN = 5;

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
  virtual void processPostArgument    (const char * key, const char * value, const byte flags);
  };  // end of myServerClass

myServerClass myServer;

// -----------------------------------------------
//  User handlers
// -----------------------------------------------

void myServerClass::processPostType (const char * key, const byte flags)
  {
  println(F("HTTP/1.1 200 OK"));
  println(F("Content-Type: text/html\n"
            "Connection: close\n"
            "Server: HTTPserver/1.0.0 (Arduino)"));
  println();   // end of headers
  println (F("<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Arduino test</title>\n"
             "</head>\n"
             "<body>\n"));

  } // end of processPostType

void myServerClass::processPostArgument (const char * key, const char * value, const byte flags)
  {
  if (memcmp (key, "led_", 4) == 0 && isdigit (key [4]) )
    {
    int which = atoi (&key [4]);
    if (which >= LOW_PIN && which <= HIGH_PIN)
      digitalWrite (which, HIGH);
    }
  }  // end of processPostArgument

// -----------------------------------------------
//  End of user handlers
// -----------------------------------------------

void setup ()
  {
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, gateway, subnet);

  for (int i = LOW_PIN; i <= HIGH_PIN; i++)
    pinMode (i, OUTPUT);
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

  // default LEDs to off
  for (int i = LOW_PIN; i <= HIGH_PIN; i++)
    digitalWrite (i, LOW);

  myServer.begin (&client);
  while (client.connected() && !myServer.done)
    {
    while (client.available () > 0 && !myServer.done)
      myServer.processIncomingByte (client.read ());

    // do other stuff here

    }  // end of while client connected

  // send the form
  myServer.println (F("<p><form METHOD=\"post\" ACTION=\"/activate_leds\">"));
  for (int i = LOW_PIN; i <= HIGH_PIN; i++)
    {
    myServer.print (F("<br>LED: "));
    myServer.print (i);
    myServer.print (F(" <input type=checkbox name=\"led_"));
    myServer.print (i);
    myServer.print (F("\" value=1 "));
    // check the box if the pin is high
    if (digitalRead (i) == HIGH)
      myServer.print (F("checked "));
    myServer.println (F(">"));
    }
  // submit button
  myServer.println (F("<p><input Type=submit Name=Submit Value=\"Process\">"));
  myServer.println (F("</form>"));

  myServer.println(F("<hr>OK, done."));

  myServer.println (F("</body>\n"
                      "</html>"));

  myServer.flush ();

  // give the web browser time to receive the data
  delay(1);
  // close the connection:
  client.stop();

  }  // end of loop
