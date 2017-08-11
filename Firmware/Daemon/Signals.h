#ifndef SIGNALS_H
#define SIGNALS_H

#include "Generals.h"

#define DEBUGMSGTL(x)
class Signals
{

public:

    static int     RECONFIG;


    explicit Signals();

    void registerOne(int sig, void (* handle)(int) );
    void registerAll();

    static void shutDown(int a);
    static void reconfig(int a);
    static void dump(int a);

    static void catchRandomSignal(int a);

};

#endif // SIGNALS_H
