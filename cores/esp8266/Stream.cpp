/*
 Stream.cpp - adds parsing methods to Stream class
 Copyright (c) 2008 David A. Mellis.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 Created July 2011
 parsing functions based on TextFinder library by Michael Margolis
 */

#include <Arduino.h>
#include <Stream.h>
#include <StreamString.h>

#define PARSE_TIMEOUT 1000  // default number of milli-seconds to wait
#define NO_SKIP_CHAR  1  // a magic char not found in a valid ASCII numeric field

// private method to read stream with timeout
int Stream::timedRead() {
    int c;
    _startMillis = millis();
    do {
        c = read();
        if(c >= 0)
            return c;
        if(_timeout == 0)
            return -1;
        yield();
    } while(millis() - _startMillis < _timeout);
    return -1;     // -1 indicates timeout
}

// private method to peek stream with timeout
int Stream::timedPeek() {
    int c;
    _startMillis = millis();
    do {
        c = peek();
        if(c >= 0)
            return c;
        if(_timeout == 0)
            return -1;
        yield();
    } while(millis() - _startMillis < _timeout);
    return -1;     // -1 indicates timeout
}

// returns peek of the next digit in the stream or -1 if timeout
// discards non-numeric characters
int Stream::peekNextDigit(bool detectDecimal) {
    int c;
    while(1) {
        c = timedPeek();
        if( c < 0 || // timeout
            c == '-' ||
            ( c >= '0' && c <= '9' ) ||
            ( detectDecimal && c == '.' ) ) {
            return c;
        }
        read();  // discard non-numeric
    }
}

// Public Methods
//////////////////////////////////////////////////////////////

void Stream::setTimeout(unsigned long timeout)  // sets the maximum number of milliseconds to wait
{
    _timeout = timeout;
}

// find returns true if the target string is found
bool Stream::find(const char *target) {
    return findUntil(target, (char*) "");
}

// reads data from the stream until the target string of given length is found
// returns true if target string is found, false if timed out
bool Stream::find(const char *target, size_t length) {
    return findUntil(target, length, NULL, 0);
}

// as find but search ends if the terminator string is found
bool Stream::findUntil(const char *target, const char *terminator) {
    return findUntil(target, strlen(target), terminator, strlen(terminator));
}

// reads data from the stream until the target string of the given length is found
// search terminated if the terminator string is found
// returns true if target string is found, false if terminated or timed out
bool Stream::findUntil(const char *target, size_t targetLen, const char *terminator, size_t termLen) {
    size_t index = 0;  // maximum target string length is 64k bytes!
    size_t termIndex = 0;
    int c;

    if(*target == 0)
        return true;   // return true if target is a null string
    while((c = timedRead()) > 0) {

        if(c != target[index])
            index = 0; // reset index if any char does not match

        if(c == target[index]) {
            //////Serial.print("found "); Serial.write(c); Serial.print("index now"); Serial.println(index+1);
            if(++index >= targetLen) { // return true if all chars in the target match
                return true;
            }
        }

        if(termLen > 0 && c == terminator[termIndex]) {
            if(++termIndex >= termLen)
                return false;       // return false if terminate string found before target string
        } else
            termIndex = 0;
    }
    return false;
}

// returns the first valid (long) integer value from the current position.
// initial characters that are not digits (or the minus sign) are skipped
// function is terminated by the first character that is not a digit.
long Stream::parseInt() {
    return parseInt(NO_SKIP_CHAR); // terminate on first non-digit character (or timeout)
}

// as above but a given skipChar is ignored
// this allows format characters (typically commas) in values to be ignored
long Stream::parseInt(char skipChar) {
    boolean isNegative = false;
    long value = 0;
    int c;

    c = peekNextDigit(false);
    // ignore non numeric leading characters
    if(c < 0)
        return 0; // zero returned if timeout

    do {
        if(c == skipChar)
            ; // ignore this character
        else if(c == '-')
            isNegative = true;
        else if(c >= '0' && c <= '9')        // is c a digit?
            value = value * 10 + c - '0';
        read();  // consume the character we got with peek
        c = timedPeek();
    } while((c >= '0' && c <= '9') || c == skipChar);

    if(isNegative)
        value = -value;
    return value;
}

// as parseInt but returns a floating point value
float Stream::parseFloat() {
    return parseFloat(NO_SKIP_CHAR);
}

// as above but the given skipChar is ignored
// this allows format characters (typically commas) in values to be ignored
float Stream::parseFloat(char skipChar) {
    boolean isNegative = false;
    boolean isFraction = false;
    long value = 0;
    int c;
    float fraction = 1.0f;

    c = peekNextDigit(true);
    // ignore non numeric leading characters
    if(c < 0)
        return 0; // zero returned if timeout

    do {
        if(c == skipChar)
            ; // ignore
        else if(c == '-')
            isNegative = true;
        else if(c == '.')
            isFraction = true;
        else if(c >= '0' && c <= '9') {      // is c a digit?
            value = value * 10 + c - '0';
            if(isFraction)
                fraction *= 0.1f;
        }
        read();  // consume the character we got with peek
        c = timedPeek();
    } while((c >= '0' && c <= '9') || c == '.' || c == skipChar);

    if(isNegative)
        value = -value;
    if(isFraction)
        return value * fraction;
    else
        return value;
}

// read characters from stream into buffer
// terminates if length characters have been read, or timeout (see setTimeout)
// returns the number of characters placed in the buffer
// the buffer is NOT null terminated.
//
size_t Stream::readBytes(char *buffer, size_t length) {
    IAMSLOW();

    size_t count = 0;
    while(count < length) {
        int c = timedRead();
        if(c < 0)
            break;
        *buffer++ = (char) c;
        count++;
    }
    return count;
}

// as readBytes with terminator character
// terminates if length characters have been read, timeout, or if the terminator character  detected
// returns the number of characters placed in the buffer (0 means no valid data found)

size_t Stream::readBytesUntil(char terminator, char *buffer, size_t length) {
    if(length < 1)
        return 0;
    size_t index = 0;
    while(index < length) {
        int c = timedRead();
        if(c < 0 || c == terminator)
            break;
        *buffer++ = (char) c;
        index++;
    }
    return index; // return number of characters, not including null terminator
}

String Stream::readString() {
    String ret;
    int c = timedRead();
    while(c >= 0) {
        ret += (char) c;
        c = timedRead();
    }
    return ret;
}

String Stream::readStringUntil(char terminator) {
    String ret;
    int c = timedRead();
    while(c >= 0 && c != terminator) {
        ret += (char) c;
        c = timedRead();
    }
    return ret;
}

String Stream::readStringUntil(const char* terminator, uint32_t untilTotalNumberOfOccurrences) {
    String ret;
    int c;
    uint32_t occurrences = 0;
    size_t termLen = strlen(terminator);
    size_t termIndex = 0;
    size_t index = 0;

    while ((c = timedRead()) > 0) {
        ret += (char) c;
        index++;

        if (terminator[termIndex] == c) {
            if (++termIndex == termLen && ++occurrences == untilTotalNumberOfOccurrences) {
                // don't include terminator in returned string
                ret.remove(index - termIndex, termLen);
                break;
            }
        } else {
            termIndex = 0;
        }
    }

    return ret;
}

String Stream::readStreamString(const ssize_t maxLen ,const oneShotMs::timeType timeoutMs) {
    String ret;
    S2Stream stream(ret);
    sendGeneric(&stream, maxLen, -1, timeoutMs);
    return ret;
}

String Stream::readStreamStringUntil(const int readUntilChar, const oneShotMs::timeType timeoutMs) {
    String ret;
    S2Stream stream(ret);
    sendGeneric(&stream, -1, readUntilChar, timeoutMs);
    return ret;
}

String Stream::readStreamStringUntil (const char* terminatorString, uint32_t untilTotalNumberOfOccurrences, const oneShotMs::timeType timeoutMs) {
    String ret;
    S2Stream stream(ret);
    uint32_t occurrences = 0;
    size_t termLen = strlen(terminatorString);
    size_t termIndex = 0;
    // Serial.printf("S %s\n",terminatorString);
    while(1){
        size_t read = sendGeneric(&stream, -1, terminatorString[termIndex], timeoutMs);
        // Serial.printf("r %d, l %d, ti %d\n", read, termLen, termIndex);
        if(getLastSendReport() != Report::Success) {
            Serial.printf("Error %d\n", (int) getLastSendReport());
            break;
        }
        if(termIndex == termLen - 1){
            // Serial.printf("m %d\n", occurrences);
            if(++occurrences == untilTotalNumberOfOccurrences){
                break;
            }else{
                ret += terminatorString;
                termIndex = 0;
                continue;
            }
        }
        int c = timedPeek();
        // Serial.printf("c %c %02X\n", c, c);
        if( c >= 0 && c != terminatorString[++termIndex]){
            ret += String(terminatorString).substring(0, termIndex);
            termIndex = 0;
            continue;
        };
        if(c < 0 || (read == 0 && termIndex == 0)) break;
    }
    
    return ret;
}



// read what can be read, immediate exit on unavailable data
// prototype similar to Arduino's `int Client::read(buf, len)`
int Stream::read (uint8_t* buffer, size_t maxLen)
{
    IAMSLOW();

    size_t nbread = 0;
    while (nbread < maxLen && available())
    {
        int c = read();
        if (c == -1)
            break;
        buffer[nbread++] = c;
    }
    return nbread;
}
