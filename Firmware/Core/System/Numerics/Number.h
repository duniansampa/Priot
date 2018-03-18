#ifndef NUMBER_H_
#define NUMBER_H_

/*!
 * \brief     Number class.
 * \details   A class to define Number operations.
 * \author    Dunian Coutinho Sampa
 * \version   1.0
 * \date      2015-2016
 * \pre       Have a Linux and C/C++ environment configured.
 * \bug       No bug.
 * \warning   N/A
 * \copyright DSPX LTDA.
 */

#include "../Base/Object.h"
#include "../Base/String.h"


//! Implementations for baselib features.
namespace baselib {

//! A class to define Number operations.
class Number : public Object {
private:
    Number * ptrOfSubClass;
protected:
    void Super(Number * subClass){
        Object::Super(this);
        this->ptrOfSubClass = subClass;
    }
public:
    //static string TYPE;

    Number(){}

    virtual ~Number(){}

    //Returns the value of the specified number as a _u64.
    virtual _s64 bigLongValue() = 0;

    //Returns the value of the specified number as a _ubyte.
    virtual _ubyte byteValue() = 0;

    //Returns the value of the specified number as a char.
    virtual char charValue() = 0;

    //Returns the value of the specified number as a double.
    virtual double doubleValue() = 0;

    //Returns the value of the specified number as a float.
    virtual float floatValue() = 0;

    //Returns the value of the specified number as a int.
    virtual int intValue() = 0;

    //Returns the value of the specified number as a long.
    virtual long longValue() = 0;

    //Returns the value of the specified number as a short.
    virtual short shortValue() = 0;

    //Returns the pointer of this Number.
    Number * getSubClass(){
        return this->ptrOfSubClass;
    }

};

/*!
* \class Number
* \brief A class to perform Number operations.
*/
}

#endif /* NUMBER_H_ */


