/*
 * Copyright (c) 2014, Intel Corporation
 *
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of the Intel Corporation nor the names of its 
 *   contributors may be used to endorse or promote products derived from 
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @author Santi Galan, Ramon Matas Navarro
 **/

#ifndef _CLOCKABLE_
#define _CLOCKABLE_

// C++/STL
#include <cstring>
#include <list>

// ASIM core
#include "asim/clockserver.h"

using namespace std;


/**
 * Interface that must implement all the classes that wants to be clocked
 **/
typedef class ASIM_CLOCKABLE_CLASS *ASIM_CLOCKABLE;

// Static array used to map a phase to a certain skew
static UINT32 clkEdges2skew[NUM_CLK_EDGES] = {0, 50};

class ASIM_CLOCKABLE_CLASS : public TRACEABLE_CLASS
{
    
  private:

    static ASIM_CLOCK_SERVER_CLASS clockServer;	

    /** The module has been registered directly to a domain? */
    bool registered;
    bool registeredDral;
    
    /** FIX ME! The following three variables correspond to the first frequency registered.
        The thread is ok, because we want a module to be clocked always by the same thread,
        as for the domain and clockInfo would be nice to have more than one instance. Anyway,
        having only one (the first registered) will work because they are only used to show
        some info (current cpu cycle, module frequency...) */
    
    /** Class that holds all the timing information related to this cloclable element */
    CLOCK_REGISTRY clockInfo;    
    /** Thread associated with this module */
    ASIM_CLOCKSERVER_THREAD thread;    
    /** Domain to which the clockserver is registered to */
    CLOCK_DOMAIN domain;
        
    ASIM_CLOCKABLE parent;
    
    UINT64 nCycles;
    UINT64 nCyclesMin;
    UINT64 nCyclesMax;
    UINT64 nClocked;
    UINT64 nCyclesWrapAround;
    UINT64 nWrapAround;
    
    // List of callbacks created by this clockable just to be able to delete them
    list<CLOCK_CALLBACK_INTERFACE> cbL;

  protected:
    /*
     * a thread object, if this module wants to run in parallel.
     * This is left protected, so that derived classes can allocate
     * threads in non-standard ways if they choose.
     */
    ASIM_SMP_THREAD_HANDLE host_thread;

  public:

    ASIM_CLOCKABLE_CLASS(ASIM_CLOCKABLE_CLASS* p,
                         bool create_thread = false) :
        registered(false),
        registeredDral(false),
        thread(NULL),
        parent(p),
        nCycles(0),
        nCyclesMin(UINT64_MAX),
        nCyclesMax(0),
        nClocked(0),
        nCyclesWrapAround(0),
        nWrapAround(0),
        host_thread(NULL)
    { }

    virtual ~ASIM_CLOCKABLE_CLASS()
    {
        for(list<CLOCK_CALLBACK_INTERFACE>::iterator it = cbL.begin(); it != cbL.end(); it++)
        {
            delete (*it);
        }
        if (host_thread)
        {
            delete host_thread;
        }
    }

    /**
     * Methods to create clockserver callbacks.
     *
     **/
    template<class T> CLOCK_CALLBACK_INTERFACE
    newCallbackPhase(T *obj, void (T::*meth)(UINT64, CLK_EDGE), CLK_EDGE ed = HIGH)
    {
        CLOCK_CALLBACK_INTERFACE cb = new ClockCallBackPhase<T>(obj, meth, ed);
        cbL.push_back(cb);
        return cb;
    }

    template<class T> CLOCK_CALLBACK_INTERFACE
    newCallbackPhase(T *obj, void (T::*meth)(PHASE), CLK_EDGE ed = HIGH)
    {
        CLOCK_CALLBACK_INTERFACE cb = new ClockCallBackPhase<T>(obj, meth, ed);
        cbL.push_back(cb);
        return cb;
    }
    
    template<class T> CLOCK_CALLBACK_INTERFACE
    newCallback(T *obj, void (T::*meth)(UINT64))
    {
        CLOCK_CALLBACK_INTERFACE cb = new ClockCallBack<T>(obj, meth);
        cbL.push_back(cb);
        return cb;
    }
    
    
    bool IsRegistered() { return registered; }
    
    bool IsRegisteredDral() { return registeredDral; }
    void SetRegisteredDral()
    {
        ASSERTX(!registeredDral);
        registeredDral = true;
    }

    /**
     * GetHostThread() returns the handle to the thread this module wants to run in.
     * This thread handle is typically passed to clockserver.RegisterClock()
     * to get clock server callbacks in a parallel thread.
     */
    virtual ASIM_SMP_THREAD_HANDLE GetHostThread(void) { return host_thread; }
    virtual void SetHostThread(ASIM_SMP_THREAD_HANDLE h)  { host_thread = h; }
       
    /** 
     * Method to access to the global instance of the clock server
     *
     * @return The system clock server
     **/
    static ASIM_CLOCK_SERVER GetClockServer(void) { return &clockServer; }

    /** 
     * Methods to create a new clock domain.
     *
     * @param name  Unique name of the clock domain.
     * @param freq/freqs  Working frequencies in GHz
     * @param tHandle   Handle of the thread that will be used for clocking
     *                  the modules registed to the created domain. If NULL is passed,
     *                  use the thread that was created in the costructor, if any.
     *                  Otherwise, just use the global default thread.
     **/
    void NewClockDomain(
        string name,
        float freq,
        ASIM_SMP_THREAD_HANDLE tHandle = NULL)
    {
        ASSERTX(freq > (float)0);
        
        list<float> tmp_list;
        tmp_list.push_back(freq);
        clockServer.NewClockDomain(name, tmp_list, tHandle ? tHandle : GetHostThread());
    }
    
    void NewClockDomain(
        string name,
        list<float> freqs,
        ASIM_SMP_THREAD_HANDLE tHandle = NULL)
    {
        list<float>::iterator it = freqs.begin();
        while(it != freqs.end())
        {
            ASSERTX(*it > (float)0);
            ++it;
        }
        
        clockServer.NewClockDomain(name, freqs, tHandle ? tHandle : GetHostThread());
    }
    
    /** 
     * Methods to dynamically change the working frequency of a clock domain.
     *
     * @param name Name of the clock domain.
     * @param freq New working frequency.
     **/
    void SetDomainFrequency(string name, float freq){ clockServer.SetDomainFrequency(name, freq); }
             
    /**
     * Adds a new model to be clocked in the given domain.
     *
     * @param domainName Name of the domain to which the clockable module belongs to.
     * @param skew Clocking skew within this domain.
     * @param tHandle Handle of the thread that will clock the module
     *                (a NULL value means the one created in the constructor,
     *                 or failing that, the domain's default thread)
     * @param referenceDomain Set as reference clock domain for this module.
     **/
    void RegisterClock(
        string domainName,
        UINT32 skew = 0,
        ASIM_SMP_THREAD_HANDLE tHandle = NULL,
        bool referenceDomain = false)
    {        
        // The following assertion has been removed to allow a module to get registered to be clocked
        // at different domains/skews        
        // ASSERT(!registered, "This module already belongs to a domain!");
        
        VERIFYX(skew <= 50);
        
        if(registered) registered = !referenceDomain;
        
        CLOCK_DOMAIN _domain = clockServer.RegisterClock(this, domainName, skew, tHandle ? tHandle : GetHostThread());
        if(!registered)
        {
            domain = _domain;
            registered = true;
        }
    }
              
    /**
     * Adds a new model to be clocked in the given domain.
     *
     * @param domainName Name of the domain to which the clockable module belongs to.
     * @param tHandle Handle of the thread that will clock the module.
     **/
    void RegisterClock(
        string domainName,
        ASIM_SMP_THREAD_HANDLE tHandle)
    {
        RegisterClock(domainName, /*skew=*/0, tHandle);
    }

    /**
     * Adds a new model to be clocked in the given domain specifying the method to be called.
     *
     * @param domainName Name of the domain to which the clockable module belongs to.
     * @param cb Callback to the method that will be called.
     * @param skew Clocking skew within this domain.
     * @param tHandle Handle of the thread that will clock the module
     *                (a NULL value means use the one created in the construor,
     *                 or if none created in the constructure use the default domain's thread)
     * @param referenceDomain Set as reference clock domain for this module.
     **/
    void RegisterClock(
        string domainName,
        CLOCK_CALLBACK_INTERFACE cb,
        UINT32 skew = 0,
        ASIM_SMP_THREAD_HANDLE tHandle = NULL,
        bool referenceDomain = false)
    {
        // The following assertion has been removed to allow a module to get registered to be clocked
        // at different domains/skews        
        // ASSERT(!registered, "This module already belongs to a domain!");
        
        VERIFYX(skew <= 50);
        
        if(registered) registered = !referenceDomain;

        // The current skew is scaled to support phases 
        CLOCK_DOMAIN _domain =
          clockServer.RegisterClock(
            this, domainName, cb,
            skew + clkEdges2skew[cb->getClkEdge()],
            tHandle ? tHandle : GetHostThread()
          );
        if(!registered)
        {
            domain = _domain;
            registered = true;
        }      
    }

    /**
     * Adds a new model to be clocked in the given domain specifying the clock edge and method to be called.
     *
     * @param domainName Name of the domain to which the clockable module belongs to.
     * @param cb Callback to the method that will be called, it must be a phased callback.
     * @param ed Clock edge when the module will be clocked.
     * @param skew Clocking skew within this domain.
     * @param tHandle Handle of the thread that will clock the module
     *                (a NULL value means use the one from the constructor,
     *                 or failing that, the default domain's thread)
     * @param referenceDomain Set as reference clock domain for this module.
     **/
    void RegisterClock(
        string domainName,
        CLOCK_CALLBACK_INTERFACE cb,
        CLK_EDGE ed,
        UINT32 skew = 0,
        ASIM_SMP_THREAD_HANDLE tHandle = NULL,
        bool referenceDomain = false)
    {
        cb->setClkEdge(ed);
        RegisterClock(domainName, cb, skew, tHandle ? tHandle : GetHostThread(), referenceDomain);
    }
   
    /**
     * Method to assign to the clockable element all the clocking info attached to it
     * This method is called by the clockserver and it's only available for the registered
     * modules.
     *
     * @param info All the clockserver related info
     */
    void SetClockInfo(CLOCK_REGISTRY info)
    {
        // By now, only one clockInfo per clockable, although it can be registered at
        // different frequencies
        if(!registered)
        {
            clockInfo = info;
        }
    }
   
    /**
     * Method to get the clockable element with all the clocking info attached to it
     */    
    inline CLOCK_REGISTRY GetClockInfo()
    {
        if(registered)
        {
            ASSERTX(clockInfo);
            return clockInfo;
        }
        else
        {
            //ASSERTX(parent);
            if (!parent)
            {
                //That should only happen when the RegisterDralTurnOn method
                //is called from models that do not use the Clock server
                return NULL;
            }
            return parent->GetClockInfo();
        }
    }
    
    /**
     * Method to assign to the clocking thread to this module.
     */
    void SetClockingThread(ASIM_CLOCKSERVER_THREAD _thread) { thread = _thread; }

    /**
     * Method to obtain the clocking thread of this module.
     */    
    inline ASIM_CLOCKSERVER_THREAD GetClockingThread()
    {
        if(registered)
        {
            ASSERTX(thread);
            return thread;
        }
        else
        {
            ASSERTX(parent);
            return parent->GetClockingThread();
        }
    }

    /** 
     * Method to obtain the current clock server base cycle where this
     * clockable element is going to be clocked
     *
     * @return Current ClockServer base clock
     */
    inline UINT64 GetCurrentBaseCycle(void)
    {
        if(registered)
        {
            ASSERTX(clockInfo);
            return clockInfo->nBaseCycle;
        }
        else
        {
            ASSERTX(parent);
            return parent->GetCurrentBaseCycle();
        }
    }

    /** 
     * Method to obtain the current clockable module cycle
     *
     * @return Current clockable cycle
     */    
    inline UINT64 GetCurrentCycle(void)
    {
        if(registered)
        {
            ASSERTX(clockInfo);
            return clockInfo->nCycle;
        }
        else
        {
            ASSERTX(parent);
            return parent->GetCurrentCycle();
        }
    }

    /**
     * Virtual function that must be implemented by all registered 
     * classes to an clock server
     *
     * @param cycle Current execution cycle
     **/
    virtual void Clock(UINT64 cycle)
    {
        cout << "what am I doing here!?" << endl;
    }; 

    /**
     * Methods to register rate matchers.
     * IMPORTANT!! They are internally called by the rate matcher
     * constructor, so don't use them from a module!!
     *
     * @param Reader or Writer rate matcher
     **/
    void RegisterRateMatcherWriter(RATE_MATCHER w);
    void RegisterRateMatcherReader(RATE_MATCHER r);    
    
    
    #if defined(HOST_DUNIX) | defined(HOST_LINUX_X86)
    
    /** 
     * Method to identify the elem to dump profiling information 
     * Transient implementation. At the moment we only use this
     * method on Alpha, due to profile is not implemented on x86
     * yet. --> a x86 version has been also implemented!
     *
     * @return An identifyer to be dumped by the profiling file
     **/
    virtual const char *ProfileId(void) const = 0; 
    
    #endif
        
    /**
     * Updates the number of cycles spent by the clock routine. This
     * information is for profiling purpose.
     *
     * @param cycles Number of cycles spent by the clock method
     **/
    void IncCyclesSpent(UINT64 cycles)
    { 
    	if(cycles < nCyclesMin && cycles > 0) nCyclesMin = cycles;
    	if(cycles > nCyclesMax) nCyclesMax = cycles;
    	nCycles += cycles;
    	nClocked++;
    }

    /**
     * Updates the stats about the wrap around counted cycles
     *
     * @param cycles Cycles counted in wrap around lecture
     **/
    void IncWrapAround(UINT64 cycles) 
    { 
    	nCyclesWrapAround += cycles;
    	nWrapAround++; 
    } 

    UINT64 GetCycles(void) { return nCycles; } 
    UINT64 GetCyclesMin(void) { return nCyclesMin; } 
    UINT64 GetCyclesMax(void) { return nCyclesMax; }
    UINT64 GetNumInvocations(void) { return nClocked; }
    UINT64 GetWrapAroundCycles(void) { return nCyclesWrapAround; } 
    UINT64 GetWrapAround(void) { return nWrapAround; }

    /**
     * Virtual method that the clock server will call when the DRAL events are
     * turned on. This method is useful to dump the internal status of any
     * structure (that must inherit ASIM_CLOCKABLE) to the events file just
     * after turning the dral server on.
     **/
    virtual void DralEventsTurnedOn() {}

    void RegisterDralTurnOn()
    {
        CLOCK_REGISTRY info = GetClockInfo();
        //If the clock server is not actually working, we do not get registered
        //(by now, without prompting any warning)
        if (info)
        {
            info->lEventsTurnOn.push_back(this);
        }
    }
   
};

#endif /* _CLOCKABLE_ */
