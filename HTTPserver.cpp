/*

 Arduino tiny web server.

 Copyright 2015 Nick Gammon.

 Version: 1.3

   Change history
   --------------
   1.1 - Fixed header values to not be percent-encoded, fixed cookie issues.
         Also various bugfixes.
   1.2 - Added buffering of writes.
   1.3 - Removed trailing space from header and cookie values


   http://www.gammon.com.au/forum/?id=12942

 PERMISSION TO DISTRIBUTE

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.


 LIMITATION OF LIABILITY

 The software is provided "as is", without warranty of any kind, express or implied,
 including but not limited to the warranties of merchantability, fitness for a particular
 purpose and noninfringement. In no event shall the authors or copyright holders be liable
 for any claim, damages or other liability, whether in an action of contract,
 tort or otherwise, arising from, out of or in connection with the software
 or the use or other dealings in the software.

*/

#include <Arduino.h>
#include <HTTPserver.h>

// ---------------------------------------------------------------------------
// clear the key/value buffers ready for a new key/value
// ---------------------------------------------------------------------------
void HTTPserver::clearBuffers ()
  {
  keyBuffer [0]   = 0;
  valueBuffer [0] = 0;
  keyBufferPos    = 0;
  valueBufferPos  = 0;
  bodyBufferPos   = 0;
  encodePhase     = ENCODE_NONE;
  flags           = FLAG_NONE;
  } // end of HTTPserver::clearBuffers

// ---------------------------------------------------------------------------
// switch states (could add state-change debugging in here)
// ---------------------------------------------------------------------------
void HTTPserver::newState (StateType what)
  {
  state = what;
  } // end of HTTPserver::newState

// ---------------------------------------------------------------------------
// add a character to the key buffer - straight text
// ---------------------------------------------------------------------------
void HTTPserver::addToKeyBuffer (const byte inByte)
  {
  if (keyBufferPos >= MAX_KEY_LENGTH)
    {
    flags |= FLAG_KEY_BUFFER_OVERFLOW;
    return;
    }  // end of overflow
  keyBuffer [keyBufferPos++] = inByte;
  keyBuffer [keyBufferPos] = 0;  // trailing null-terminator
  } // end of HTTPserver::addToKeyBuffer

// ---------------------------------------------------------------------------
// add a character to the value buffer - percent-encoded (if wanted)
// ---------------------------------------------------------------------------
 void HTTPserver::addToValueBuffer (byte inByte, const bool percentEncoded)
  {
  if (valueBufferPos >= MAX_VALUE_LENGTH)
    {
    flags |= FLAG_VALUE_BUFFER_OVERFLOW;
    return;
    }  // end of overflow

  // look for stuff like "foo+bar" (turn the "+" into a space)
  // and also "foo%21bar" (turn %21 into one character)
  if (percentEncoded)
    {
    switch (encodePhase)
      {

      // if in "normal" mode, turn a "+" into a space, and look for "%"
      case ENCODE_NONE:
        if (inByte == '+')
          inByte = ' ';
        else if (inByte == '%')
          {
          encodePhase = ENCODE_GOT_PERCENT;
          return;  // no addition to buffer yet
          }
        break;

      // we had the "%" last time, this should be the first hex digit
      case ENCODE_GOT_PERCENT:
        if (isxdigit (inByte))
          {
          byte c = toupper (inByte) - '0';
          if (c > 9)
            c -= 7;  // Fix A-F
          encodeByte = c << 4;
          encodePhase = ENCODE_GOT_FIRST_CHAR;
          return;  // no addition to buffer yet
          }
        // not a hex digit, give up
        encodePhase = ENCODE_NONE;
        flags |= FLAG_ENCODING_ERROR;
        break;

      // this should be the second hex digit
      case ENCODE_GOT_FIRST_CHAR:
        if (isxdigit (inByte))
          {
          byte c = toupper (inByte) - '0';
          if (c > 9)
            c -= 7;  // Fix A-F
          inByte = encodeByte | c;
          }
        else
          flags |= FLAG_ENCODING_ERROR;

        // done with encoding it, or not a hex digit
        encodePhase = ENCODE_NONE;

      } // end of switch on encodePhase
    } // end of percent-encoded

  // add to value buffer, encoding has been dealt with
  valueBuffer [valueBufferPos++] = inByte;
  valueBuffer [valueBufferPos] = 0;  // trailing null-terminator
  } // end of HTTPserver::addToValueBuffer

// ---------------------------------------------------------------------------
// add a character to the body buffer - raw binary
// ---------------------------------------------------------------------------
void HTTPserver::addToBodyBuffer (const byte inByte)
  {
  if (bodyBufferPos >= BODY_CHUNK_LENGTH)
    {
      // pass current chunk to the application and empty it
      processBodyChunk (bodyBuffer, bodyBufferPos, flags);
      bodyBufferPos = 0;
    }  // end of overflow
    bodyBuffer [bodyBufferPos++] = inByte;
  } // end of HTTPserver::addToBodyBuffer

// ---------------------------------------------------------------------------
//  handleSpace - we have an incoming space
// ---------------------------------------------------------------------------

// in the state machine handlers the symbols { } indicate where we think we are
void HTTPserver::handleSpace ()
  {
  switch (state)
    {

    // GET{   }/pathname/filename?foo=bar&fubar=true HTTP/1.1
    case SKIP_GET_SPACES_1:
    // GET /pathname/filename?foo=bar&fubar=true{   }HTTP/1.1
    case SKIP_GET_SPACES_2:
    // GET /pathname/filename?foo=bar&fubar=true HTTP/1.1{   }
    case SKIP_TO_END_OF_LINE:
    // Cookie:{ }foo=bar;
    case SKIP_COOKIE_SPACES:
      break;  // ignore these spaces

    // GET{ }/pathname/filename?foo=bar&fubar=true HTTP/1.1
    case GET_LINE:
      processPostType (keyBuffer, flags);
      // see if it is a POST type
      postRequest = strcmp (keyBuffer, "POST") == 0;
      newState (SKIP_GET_SPACES_1);
      clearBuffers ();
      break;

    // GET /pathname/filename?foo=bar&fubar=true{ }HTTP/1.1
    case GET_PATHNAME:
      processPathname (valueBuffer, flags);
      newState (SKIP_GET_SPACES_2);
      clearBuffers ();
      break;

    // GET /pathname/filename?foo=bar&fubar=true{ }HTTP/1.1
    case GET_ARGUMENT_NAME:
      processGetArgument (keyBuffer, valueBuffer, flags);
      newState (SKIP_GET_SPACES_2);
      clearBuffers ();
      break;

    // GET /pathname/filename?foo{ }HTTP/1.1
    case GET_ARGUMENT_VALUE:
      processGetArgument (keyBuffer, valueBuffer, flags);
      newState (SKIP_GET_SPACES_2);
      clearBuffers ();
      break;

    // GET /pathname/filename?foo=bar&fubar=true HTTP/1.1{ }
    case GET_HTTP_VERSION:
      processHttpVersion (keyBuffer, flags);
      newState (SKIP_TO_END_OF_LINE);
      clearBuffers ();
      break;

    // Accept-Encoding: gzip,{ }deflat
    case HEADER_VALUE:
    case COOKIE_VALUE:
      addToValueBuffer (' ', false);
      break;

    // Accept-Encoding{ }:
    // space shouldn't be there, but we'll ignore it
    case HEADER_NAME:
      break;

    default:
      break;  // do nothing

    } // end of switch on state

  } // end of HTTPserver::handleSpace

// ---------------------------------------------------------------------------
//  handleNewline - we have an incoming newline
// ---------------------------------------------------------------------------
void HTTPserver::handleNewline ()
  {

  // pretend there was a trailing space and wrap up the previous line
  if (state != SKIP_TO_END_OF_LINE &&
      state != SKIP_INITIAL_LINES &&
      state != HEADER_VALUE &&  // don't have trailing space on header value
      state != COOKIE_VALUE)    // nor on cookie value
    handleSpace ();

  switch (state)
    {
    // default is the start of a new header line
    default:
      clearBuffers ();
      newState (START_LINE);
      break;

    // ignore blank lines before the GET/POST line
    case SKIP_INITIAL_LINES:
      break;

    // a blank line on its own signals switching to the POST key/values or binary body
    case START_LINE:
      clearBuffers ();
      newState (binaryBody ? BODY : POST_NAME);
      break;

    // wrap up this POST key/value and start a new one
    case POST_NAME:
    case POST_VALUE:
      if (keyBufferPos > 0)
        processPostArgument (keyBuffer, valueBuffer, flags);
      newState (POST_NAME);
      clearBuffers ();
      break;

    // end of a header value, start looking for a new header
    case HEADER_VALUE:
      processHeaderArgument (keyBuffer, valueBuffer, flags);
      // remember the content length for the POST data
      if (strcasecmp (keyBuffer, "Content-Length") == 0)
        contentLength = atol (valueBuffer);
      if (strcasecmp (keyBuffer, "Content-Type") == 0 && strcasecmp (valueBuffer, "application/octet-stream") == 0)
        binaryBody = true;
      clearBuffers ();
      newState (START_LINE);
      break;

    case COOKIE_VALUE:
      processCookie (keyBuffer, valueBuffer, flags);
      newState (START_LINE);
      clearBuffers ();
      break;

    } // end of switch on state

  } // end of HTTPserver::handleNewline

// ---------------------------------------------------------------------------
//  handleText - we have an incoming character other than a space or newline
// ---------------------------------------------------------------------------

// in the state machine handlers the symbols { } indicate where we think we are
void HTTPserver::handleText (const byte inByte)
  {
  switch (state)
    {
    // blank lines before GET line
    case SKIP_INITIAL_LINES:
      newState (GET_LINE);
      addToKeyBuffer (inByte);
      break;

    // {GET} /whatever/foo.htm HTTP/1.1
    case GET_LINE:
    // GET /whatever/foo.htm {HTTP/1.1}
    case GET_HTTP_VERSION:
      addToKeyBuffer (inByte);
      break;

    // {Connection}: keep-alive
    case HEADER_NAME:
      if (inByte == ':')
        {
        if (strcasecmp (keyBuffer, "Cookie") == 0)
          {
          newState (SKIP_COOKIE_SPACES);
          clearBuffers ();
          }
        else
          newState (SKIP_HEADER_SPACES);
        }
      else
        addToKeyBuffer (inByte);
      break;

    // Connection: {k}eep-alive
    case SKIP_HEADER_SPACES:
      newState (HEADER_VALUE);
      addToValueBuffer (inByte, false);
      break;

    // Connection: {keep-alive}
    case HEADER_VALUE:
      addToValueBuffer (inByte, false);
      break;

    // Cookie: foo=bar;{ }whatever=something;
    case SKIP_COOKIE_SPACES:
      newState (COOKIE_NAME);
      addToKeyBuffer (inByte);
      break;

    // Cookie: {foo}=bar;
    case COOKIE_NAME:
      if (inByte == '=')
        newState (COOKIE_VALUE);
      else
        addToKeyBuffer (inByte);
      break;

    // Cookie: foo={bar};
    case COOKIE_VALUE:
      if (inByte == ';' || inByte == ',')
        {
        processCookie (keyBuffer, valueBuffer, flags);
        newState (SKIP_COOKIE_SPACES);
        clearBuffers ();
        }
      else
        addToValueBuffer (inByte, false);
      break;

    // {foo}=bar&answer=42
    case POST_NAME:
      if (inByte == '&')
        {
        processPostArgument (keyBuffer, valueBuffer, flags);
        newState (POST_NAME);
        clearBuffers ();
        }
      else if (inByte == '=')
        newState (POST_VALUE);
      else
        addToKeyBuffer (inByte);
      break;

    // foo={bar}&answer=42
    case POST_VALUE:
      if (inByte == '&')
        {
        processPostArgument (keyBuffer, valueBuffer, flags);
        newState (POST_NAME);
        clearBuffers ();
        }
      else
        addToValueBuffer (inByte, true);
      break;

    // GET {/whatever/foo.htm} HTTP/1.1
    case SKIP_GET_SPACES_1:
      newState (GET_PATHNAME);
      addToValueBuffer (inByte, true);
      break;

   // GET /pathname/filename?{foo}=bar&fubar=true
   case GET_ARGUMENT_NAME:
      if (inByte == '&')
        {
        processGetArgument (keyBuffer, valueBuffer, flags);
        newState (GET_ARGUMENT_NAME);
        clearBuffers ();
        }
      else if (inByte == '=')
        newState (GET_ARGUMENT_VALUE);
      else
        addToKeyBuffer (inByte);
      break;

    // GET /pathname/filename?foo={bar}&fubar=true
    case GET_ARGUMENT_VALUE:
      if (inByte == '&')
        {
        processGetArgument (keyBuffer, valueBuffer, flags);
        newState (GET_ARGUMENT_NAME);
        clearBuffers ();
        }
      else
        addToValueBuffer (inByte, true);
      break;

    // GET {/pathname/filename}?foo=bar&fubar=true
    case GET_PATHNAME:
      if (inByte == '?')
        {
        processPathname (valueBuffer, flags);
        newState (GET_ARGUMENT_NAME);
        clearBuffers ();
        }
      else
        addToValueBuffer (inByte, true);
      break;

    // GET /whatever/foo.htm {HTTP/1.1}
    case SKIP_GET_SPACES_2:
      newState (GET_HTTP_VERSION);
      addToKeyBuffer (inByte);
      break;

    // {C}onnection: keep-alive
    case START_LINE:
      newState (HEADER_NAME);
      addToKeyBuffer (inByte);
      break;

    // we think line is done, skip whatever we find
    case SKIP_TO_END_OF_LINE:
      break;  // ignore it
    } // end of switch on state

  }  // end of HTTPserver::handleText

// ---------------------------------------------------------------------------
//  processIncomingByte - our main sketch has received a byte from the client
// ---------------------------------------------------------------------------
void HTTPserver::processIncomingByte (const byte inByte)
  {

  // count received bytes in POST section or binary body
  if (state == POST_NAME || state == POST_VALUE || state == BODY)
    receivedLength++;

  if (state == BODY)
    {
    addToBodyBuffer (inByte);
        
    // if all received, stop now
    if (receivedLength >= contentLength)
      {
      // wrap up last partial binary chunk (always at least 1 byte here by definition)
      processBodyChunk (bodyBuffer, bodyBufferPos, flags);
      clearBuffers ();
      done = true;
      }
        
    // don't process data specially inside a binary body
    return;
    }
  
  switch (inByte)
    {
    case '\r':
      break;  // ignore carriage-return

    case ' ':
    case '\t':
      handleSpace ();    // generally switches states
      break;

    case '\n':
      handleNewline ();  // generally switches states
      break;

    default:
      handleText (inByte);     // collect text
      break;
    } // end of switch on inByte

 // see if count of content bytes is up
  if (state == POST_NAME || state == POST_VALUE)
    {
    // if all received, stop now
    if (receivedLength >= contentLength)
      {
      // handle final POST item
      if (keyBufferPos > 0)
        handleNewline ();
      done = true;
      return;
      }  // end of Content-Length reached

    // not a POST request? don't look for more data
    if (!postRequest)
      done = true;
    } // end of up to the POST states

  } // end of HTTPserver::processIncomingByte

// ---------------------------------------------------------------------------
//  begin - reset state machine to the start
// ---------------------------------------------------------------------------
void HTTPserver::begin (Print * output_)
  {
  // reset everything to initial state
  state = SKIP_INITIAL_LINES;
  encodePhase = ENCODE_NONE;
  flags = FLAG_NONE;
  postRequest = false;
  binaryBody = false;
  contentLength = 0;
  receivedLength = 0;
  sendBufferPos = 0;
  output = output_;
  clearBuffers ();
  done = false;
  } // end of HTTPserver::begin

// ---------------------------------------------------------------------------
//  write - for outputting via print, println etc.
// ---------------------------------------------------------------------------
size_t HTTPserver::write (uint8_t c)
  {
  // forget it, if they supplied no output device
  if (!output)
    return 0;

  // only buffer writes up, if a non-zero buffer length
  if (SEND_BUFFER_LENGTH > 0)
    {
    sendBuffer [sendBufferPos++] = c;
    // if full, flush it
    if (sendBufferPos >= SEND_BUFFER_LENGTH)
      flush ();
    }
  else
    output->write (c);  // otherwise write a byte at a time

  return 1;
  } // end of HTTPserver::write

void HTTPserver::flush ()
  {
  if (sendBufferPos > 0)
    {
    output->write (sendBuffer, sendBufferPos);
    sendBufferPos = 0;
    } // end of anything in buffer
  } // end of HTTPserver::flush

// ---------------------------------------------------------------------------
//  fixHTML - convert special characters such as < > and &
// ---------------------------------------------------------------------------
void HTTPserver::fixHTML (const char * message)
  {
  char c;
  while ((c = *message++))
    {
    switch (c)
      {
      case '<': print ("&lt;"); break;
      case '>': print ("&gt;"); break;
      case '&': print ("&amp;"); break;
      case '"': print ("&quot;"); break;
      default:  write (c); break;
      }  // end of switch
    } // end of while
  } // end of HTTPserver::fixHTML

// ---------------------------------------------------------------------------
//  urlEncode - convert special characters such as spaces into percent-encoded
// ---------------------------------------------------------------------------
void HTTPserver::urlEncode (const char * message)
  {
  char c;
  while ((c = *message++))
    {
    if (!isalpha (c) && !isdigit(c))
      {
      // compact conversion to hex
      write ('%');
      char x = ((c >> 4) & 0xF) | '0';
      if (x > '9')
        x += 7;
      write (x);
      x = (c & 0xF) | '0';
      if (x > '9')
        x += 7;
      write (x);
      }
    else
      write (c);
    } // end of while
  } // end of HTTPserver::urlEncode

// ---------------------------------------------------------------------------
//  setCookie - cookies only permit certain characters
// ---------------------------------------------------------------------------
void HTTPserver::setCookie (const char * name, const char * value, const char * extra)
  {
  print (F("Set-Cookie: "));
  // send the name which excludes spaces, ';', ',' or '='
  for (const char * p = name; *p; p++)
    if (*p >= '!' && *p <= '~' && *p != ';' && *p != ';' && *p != '=')
      write (*p);
  write ('=');
  // send the value which excludes ';' or ','
  for (const char * p = value; *p; p++)
    if (*p >= ' ' && *p <= '~' && *p != ';' && *p != ';')
      write (*p);
  // terminate value with semicolon and space
  print (F("; "));

  // extra stuff like:
  //  Path=/accounts; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly
  if (extra)
    print (extra);
  // end of header line
  println ();

  } // end of HTTPserver::setCookie

