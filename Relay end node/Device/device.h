#include "Arduino.h"

typedef struct {					// Radio packet format
int	nodeID;						// node identifier
int	devID;						// device identifier 
int	cmd;						// read or write
long	intVal;						// integer payload
float	fltVal;						// floating payload
char	payLoad[32];					// string payload
} Message;

class Device{
  void (*readPointer)(Message *);
  void (*writePointer)(const Message *);
  public:
    const int id;
    bool setTX;
    Device (int x, bool tx) : id(x), setTX(tx) { readPointer = &dummyRead; writePointer = &dummyWrite; }
    Device (int x, bool tx, void (*ptr)(Message *)) : id(x), setTX(tx), readPointer(ptr) { writePointer = &dummyWrite; }
    Device (int x, bool tx, void (*ptr)(Message *), void (*ptr2)(const Message *)) : id(x), setTX(tx), readPointer(ptr), writePointer(ptr2) {}
    void read(Message *mess) { readPointer(mess);}
    void write(const Message *mess) { writePointer(mess);}
    static void dummyRead(Message *mess) {}
    static void dummyWrite(const Message *mess) {}
};

