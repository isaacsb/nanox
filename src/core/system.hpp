/*************************************************************************************/
/*      Copyright 2009 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the NANOS++ library.                                    */
/*                                                                                   */
/*      NANOS++ is free software: you can redistribute it and/or modify              */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      NANOS++ is distributed in the hope that it will be useful,                   */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with NANOS++.  If not, see <http://www.gnu.org/licenses/>.             */
/*************************************************************************************/

#ifndef _NANOS_SYSTEM
#define _NANOS_SYSTEM

#include "processingelement.hpp"
#include "throttle.hpp"
#include <vector>
#include <string>
#include "schedule.hpp"
#include "threadteam.hpp"
#include "slicer.hpp"
#include "nanos-int.h"
#include "dependency.hpp"
#include "instrumentor_decl.hpp"


namespace nanos
{

// This class initializes/finalizes the library
// All global variables MUST be declared inside

   class System
   {
      friend class Scheduler;

      public:
         // constants
         typedef enum { DEDICATED, SHARED } ExecutionMode;
         typedef enum { POOL, ONE_THREAD } InitialMode;

         typedef void (*Init) ();

      private:
         // types
         typedef std::vector<PE *>         PEList;
         typedef std::vector<BaseThread *> ThreadList;
         typedef std::map<std::string, Slicer *> Slicers;
         
         // configuration variables
         int                  _numPEs;
         int                  _deviceStackSize;
         bool                 _bindThreads;
         bool                 _profile;
         bool                 _instrument;
         bool                 _verboseMode;
         ExecutionMode        _executionMode;
         InitialMode          _initialMode;
         int                  _thsPerPE;
         bool                 _untieMaster;
         bool                 _delayedStart;

         //cutoff policy and related variables
         ThrottlePolicy *     _throttlePolicy;
         Atomic<int>          _taskNum;
         Atomic<int>          _numReady;
         Atomic<int>          _idleThreads;
         Atomic<int>          _numTasksRunning;

         /*! names of the scheduling, cutoff and barrier plugins */
         std::string          _defSchedule;
         std::string          _defThrottlePolicy;
         std::string          _defBarr;
         std::string          _defInstr;

         /*! factories for scheduling, pes and barriers objects */
         sgFactory            _defSGFactory;
         peFactory            _hostFactory;
         barrFactory          _defBarrFactory;

         PEList               _pes;
         ThreadList           _workers;

         Slicers              _slicers; /**< set of global slicers */

         Instrumentor         *_instrumentor; /**< instrumentor object used in current execution */

         // disable copy constructor & assignment operation
         System( const System &sys );
         const System & operator= ( const System &sys );

         void config ();
         void loadModules();
         void start ();
         PE * createPE ( std::string pe_type, int pid );

      public:
         // constructor
         System ();
         ~System ();

         void submit ( WD &work );
         void submitWithDependencies (WD& work, size_t numDeps, Dependency* deps);
         void waitOn ( size_t numDeps, Dependency* deps);
         void inlineWork ( WD &work );

         void createWD (WD **uwd, size_t num_devices, nanos_device_t *devices,
                        size_t data_size, void ** data, WG *uwg,
                        nanos_wd_props_t *props, size_t num_copies, nanos_copy_data_t **copies );

         void createSlicedWD ( WD **uwd, size_t num_devices, nanos_device_t *devices, size_t outline_data_size,
                        void **outline_data, WG *uwg, Slicer *slicer, size_t slicer_data_size,
                        SlicerData *&slicer_data, nanos_wd_props_t *props, size_t num_copies, nanos_copy_data_t **copies );

         void duplicateWD ( WD **uwd, WD *wd );
         void duplicateSlicedWD ( SlicedWD **uwd, SlicedWD *wd );

         // methods to access configuration variable         
         void setNumPEs ( int npes ) { _numPEs = npes; }

         int getNumPEs () const { return _numPEs; }

         void setDeviceStackSize ( int stackSize ) { _deviceStackSize = stackSize; }

         int getDeviceStackSize () const {return _deviceStackSize; }

         void setBinding ( bool set ) { _bindThreads = set; }

         bool getBinding () const { return _bindThreads; }

         ExecutionMode getExecutionMode () const { return _executionMode; }

         bool getVerbose () const { return _verboseMode; }

         void setInitialMode ( InitialMode mode ) { _initialMode = mode; }
         InitialMode getInitialMode() const { return _initialMode; }

         void setThsPerPE( int ths ) { _thsPerPE = ths; }

         void setDelayedStart ( bool set) { _delayedStart = set; }

         bool getDelayedStart () const { return _delayedStart; }

         int getThsPerPE() const { return _thsPerPE; }

         int getTaskNum() const { return _taskNum.value(); }

         int getIdleNum() const { return _idleThreads.value(); }

         int getReadyNum() const { return _numReady.value(); }

         int getRunningTasks() const { return _numTasksRunning.value(); }

         int getNumWorkers() const { return _workers.size(); }

         // team related methods
         BaseThread * getUnassignedWorker ( void );
         ThreadTeam * createTeam ( unsigned nthreads, SG *scheduling=NULL, void *constraints=NULL,
                                   bool reuseCurrent=true,  TeamData *tdata = 0 );
         void endTeam ( ThreadTeam *team );
         void releaseWorker ( BaseThread * thread );

         //BUG: does not work: sigsegv on myThread
         int getSGSize() const { return myThread->getSchedulingGroup()->getSize(); }

         void setThrottlePolicy( ThrottlePolicy * policy ) { _throttlePolicy = policy; }

         bool throttleTask();

         const std::string & getDefaultSchedule() const { return _defSchedule; }

         const std::string & getDefaultThrottlePolicy() const { return _defThrottlePolicy; }

         const std::string & getDefaultBarrier() const { return _defBarr; }

         const std::string & getDefaultInstrumentor() const { return _defInstr; }

         void setDefaultSGFactory ( sgFactory factory ) { _defSGFactory = factory; }

         void setHostFactory ( peFactory factory ) { _hostFactory = factory; }

         void setDefaultBarrFactory ( barrFactory factory ) { _defBarrFactory = factory; }

         Slicer * getSlicer( const std::string &label ) const 
         { 
            Slicers::const_iterator it = _slicers.find(label);
            if ( it == _slicers.end() ) return NULL;
            return (*it).second;
         }

         Instrumentor * getInstrumentor ( void ) { return _instrumentor; }

         // TODO: Is it needed to check if _instrumentor was already set?
         void setInstrumentor ( Instrumentor *instr ) { _instrumentor = instr; }

         void registerSlicer ( const std::string &label, Slicer *slicer) { _slicers[label] = slicer; }

   };

   extern System sys;

};

#endif

