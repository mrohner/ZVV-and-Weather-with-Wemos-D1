#pragma once

#include <JsonListener.h>
#include <JsonStreamingParser.h>
const int Max_Number_Journeys = 10; //<- can be modified

//
// Class to hold ZVV reading
//
class ZVVReading
{
public:
    // Data
    int type;
    String line;
    int a_countdown;
    int countdown;
    String direction_to;
    bool operator> (const ZVVReading &rhs)
    {
        return ((countdown << 8) + line) > ((rhs.countdown << 8) + rhs.line);
    }
};


class ZVVClient: public JsonListener {
  private:
    String currentKey;
    String currentParent[10];
    int hierarchy_level = -1;
    int count = -1;
    

public:
    ZVVClient();
    ZVVReading ZVVReadings[Max_Number_Journeys];
    void doUpdate(String url);
    void sortZVVreadings();
    virtual void whitespace(char c);
    virtual void startDocument();
    virtual void key(String key);
    virtual void value(String value);
    virtual void endArray();
    virtual void endObject();
    virtual void endDocument();
    virtual void startArray();
    virtual void startObject();
    int getType(int i);
    String getLine(int i);
    int getCountdown(int i);
    String getDirection(int i);
};
