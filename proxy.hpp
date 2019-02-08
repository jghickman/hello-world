//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/proxy.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/proxy.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_PROXY_HPP
#define ISPTECH_ORB_PROXY_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/function.hpp"
#include "isptech/orb/object_id.hpp"
#include "isptech/orb/future.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Two-Way Proxy
*/
class Twoway_proxy : boost::totally_ordered<Twoway_proxy> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Twoway_proxy() = default;
    Twoway_proxy(Object_id, Interface_ptr);
    Twoway_proxy(const Twoway_proxy&) = default;
    Twoway_proxy& operator=(const Twoway_proxy&) = default;
    Twoway_proxy(Twoway_proxy&&);
    Twoway_proxy& operator=(Twoway_proxy&&);
    friend void swap(Twoway_proxy&, Twoway_proxy&);

    // Object Identity
    Object_id object() const;

    // Function Invocation
    Future<bool>::Awaitable invoke(Function, Const_buffers in, Io_buffer* outp) const;
    Future<bool>::Awaitable invoke(Function, Const_buffers in, Mutable_buffers* outp) const;
    void                    invoke_oneway(Function, Const_buffers in) const;

    // Blocking Function Invocation
    friend bool blocking_invoke(const Twoway_proxy&, Function, Const_buffers in, Io_buffer* outp);
    friend bool blocking_invoke(const Twoway_proxy&, Function, Const_buffers in, Mutable_buffers* outp);

    // Comparisons
    friend bool operator==(const Twoway_proxy&, const Twoway_proxy&);
    friend bool operator< (const Twoway_proxy&, const Twoway_proxy&);

private:
    // Daat
    Object_id       objectid;
    Interface_ptr   ifacep;
};


/*
    Two-Way Proxy Interface
*/
class Twoway_proxy::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Function Invocation
    virtual Future<bool>::Awaitable invoke(Object_id, Function, Const_buffer in, Io_buffer* outp) = 0;
    virtual Future<bool>::Awaitable invoke(Object_id, Function, Const_buffer in, Mutable_buffers* outp) = 0;
    virtual void                    invoke_oneway(Object_id, Function, Const_buffer in) = 0;

    // Blocking Function Invocation
    virtual bool blocking_invoke(Function, Const_buffers in, Io_buffer* outp) = 0;
    virtual bool blocking_invoke(Function, Const_buffers in, Mutable_buffers* outp) = 0;
};


}   // Orb
}   // Isptech

#endif  // ISPTECH_ORB_PROXY_HPP

//  $CUSTOM_FOOTER$
