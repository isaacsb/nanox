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

#include "schedule.hpp"
#include "processingelement.hpp"
#include "basethread.hpp"
#include "system.hpp"
#include "config.hpp"
#include "instrumentationmodule_decl.hpp"
#include "os.hpp"

using namespace nanos;

void SchedulerConf::config (Config &config)
{
   config.setOptionsSection ( "Core [Scheduler]", "Policy independent scheduler options"  );

   config.registerConfigOption ( "num_spins", new Config::UintVar( _numSpins ), "Determines the amount of spinning before yielding" );
   config.registerArgOption ( "num_spins", "spins" );
   config.registerEnvOption ( "num_spins", "NX_SPINS" );
}

void Scheduler::submit ( WD &wd )
{
   NANOS_INSTRUMENT( InstrumentState inst(NANOS_SCHEDULING) );
   BaseThread *mythread = myThread;

   sys.getSchedulerStats()._createdTasks++;
   sys.getSchedulerStats()._totalTasks++;

   debug ( "submitting task " << wd.getId() );

   wd.submitted();

   /* handle tied tasks */
   if ( wd.isTied() && wd.isTiedTo() != mythread ) {
      queue(wd.isTiedTo(), wd);
      return;
   }

   if ( !wd.canRunIn(*mythread->runningOn()) ) {
      queue(mythread, wd);
      return;
   }

   WD *next = getMyThreadSafe()->getTeam()->getSchedulePolicy().atSubmit( myThread, wd );

   if ( next ) {
      WD *slice;
      /* enqueue the remaining part of a WD */
      if ( !next->dequeue(&slice) ) {
         queue(mythread, *next);
      }
      switchTo ( slice );
   } else {
      /* if next == NULL wd has been enqueued by SchedulePolicy.atSubmit() */
      sys.getSchedulerStats()._readyTasks++;
   }

}

void Scheduler::updateExitStats ( WD &wd )
{
   if ( wd.isSubmitted() ) 
     sys.getSchedulerStats()._totalTasks--;
}

template<class behaviour>
inline void Scheduler::idleLoop ()
{
   NANOS_INSTRUMENT ( static InstrumentationDictionary *ID = sys.getInstrumentation()->getInstrumentationDictionary(); )
   NANOS_INSTRUMENT ( static nanos_event_key_t total_spins_key = ID->getEventKey("num-spins"); )
   NANOS_INSTRUMENT ( static nanos_event_key_t total_yields_key = ID->getEventKey("num-yields"); )
   NANOS_INSTRUMENT ( static nanos_event_key_t time_yields_key = ID->getEventKey("time-yields"); )
   NANOS_INSTRUMENT ( nanos_event_key_t Keys[3]; )
   NANOS_INSTRUMENT ( Keys[0] = total_spins_key; )
   NANOS_INSTRUMENT ( Keys[1] = total_yields_key; )
   NANOS_INSTRUMENT ( Keys[2] = time_yields_key; )

   NANOS_INSTRUMENT( InstrumentState inst(NANOS_IDLE) );

   const int nspins = sys.getSchedulerConf().getNumSpins();
   int spins = nspins;
   unsigned long total_spins = 0;  /* Number of spins by idle phase*/
   unsigned long total_yields = 0; /* Number of yields by idle phase */
   unsigned long time_yields = 0;  /* Time of yields by idle phase */

   WD *current = myThread->getCurrentWD();
   current->setIdle();
   sys.getSchedulerStats()._idleThreads++;
   for ( ; ; ) {
      BaseThread *thread = getMyThreadSafe();
      spins--;

      if ( !thread->isRunning() ) break;

      if ( thread->getTeam() != NULL ) {
         WD * next = myThread->getNextWD();

         if ( next ) {
           myThread->setNextWD(NULL);

           /* Some WDs maybe prefetched without going through the submit 
              process. Compensate the ready count for that */
           if ( !next->isSubmitted() && !next->started() ) 
             sys.getSchedulerStats()._readyTasks++;
         } else {
           if ( sys.getSchedulerStats()._readyTasks > 0 ) 
              next = behaviour::getWD(thread,current);
         } 

         if ( next ) {
            sys.getSchedulerStats()._readyTasks--;
            sys.getSchedulerStats()._idleThreads--;

            total_spins+= (nspins - spins);
            NANOS_INSTRUMENT ( nanos_event_value_t Values[3]; )
            NANOS_INSTRUMENT ( Values[0] = (nanos_event_value_t) total_spins; )
            NANOS_INSTRUMENT ( Values[1] = (nanos_event_value_t) total_yields; )
            NANOS_INSTRUMENT ( Values[2] = (nanos_event_value_t) time_yields; )
            NANOS_INSTRUMENT( sys.getInstrumentation()->raisePointEventNkvs(3, Keys, Values); )

            NANOS_INSTRUMENT( InstrumentState inst2(NANOS_RUNTIME) )
            behaviour::switchWD(thread,current, next);
            thread = getMyThreadSafe();
            NANOS_INSTRUMENT( inst2.close() );
            sys.getSchedulerStats()._idleThreads++;
            total_spins = 0;
            total_yields = 0;
            time_yields = 0;
            spins = nspins;
            continue;
         }
      }

      if ( spins == 0 ) {
        total_spins+= nspins;
        if ( sys.useYield() ) {
           total_yields++;
           unsigned long begin_yield = (unsigned long) ( OS::getMonotonicTime() * 1.0e9  );
           thread->yield();
           unsigned long end_yield = (unsigned long) ( OS::getMonotonicTime() * 1.0e9  );
           time_yields += ( end_yield - begin_yield );
        }
        spins = nspins;
      }
      else {
         thread->idle();
      }
   }
   sys.getSchedulerStats()._idleThreads--;
   current->setReady();
}

void Scheduler::waitOnCondition (GenericSyncCond *condition)
{
   NANOS_INSTRUMENT ( static InstrumentationDictionary *ID = sys.getInstrumentation()->getInstrumentationDictionary(); )
   NANOS_INSTRUMENT ( static nanos_event_key_t total_spins_key = ID->getEventKey("num-spins"); )
   NANOS_INSTRUMENT ( static nanos_event_key_t total_yields_key = ID->getEventKey("num-yields"); )
   NANOS_INSTRUMENT ( static nanos_event_key_t time_yields_key = ID->getEventKey("time-yields"); )
   NANOS_INSTRUMENT ( nanos_event_key_t Keys[3]; )
   NANOS_INSTRUMENT ( Keys[0] = total_spins_key; )
   NANOS_INSTRUMENT ( Keys[1] = total_yields_key; )
   NANOS_INSTRUMENT ( Keys[2] = time_yields_key; )

   NANOS_INSTRUMENT( InstrumentState inst(NANOS_SYNCHRONIZATION) );

   const int nspins = sys.getSchedulerConf().getNumSpins();
   int spins = nspins; 
   unsigned long total_spins = 0;  /* Number of spins by idle phase*/
   unsigned long total_yields = 0; /* Number of yields by idle phase */
   unsigned long time_yields = 0;  /* Time of yields by idle phase */

   WD * current = myThread->getCurrentWD();

   sys.getSchedulerStats()._idleThreads++;
   current->setSyncCond( condition );
   current->setIdle();
   
   BaseThread *thread = getMyThreadSafe();

   while ( !condition->check() && thread->isRunning() ) {
      spins--;
      if ( spins == 0 ) {
         total_spins+= nspins;
         condition->lock();
         if ( !( condition->check() ) ) {
            condition->addWaiter( current );

            WD *next = NULL;
            if ( sys.getSchedulerStats()._readyTasks > 0 ) {
               next = thread->getTeam()->getSchedulePolicy().atBlock( thread, current );
            }

            if ( next ) {
               sys.getSchedulerStats()._readyTasks--;
               sys.getSchedulerStats()._idleThreads--;

               NANOS_INSTRUMENT ( nanos_event_value_t Values[3]; )
               NANOS_INSTRUMENT ( Values[0] = (nanos_event_value_t) total_spins; )
               NANOS_INSTRUMENT ( Values[1] = (nanos_event_value_t) total_yields; )
               NANOS_INSTRUMENT ( Values[2] = (nanos_event_value_t) time_yields; )
               NANOS_INSTRUMENT( sys.getInstrumentation()->raisePointEventNkvs(3, Keys, Values); )

               NANOS_INSTRUMENT( InstrumentState inst2(NANOS_RUNTIME); );
               switchTo ( next );
               thread = getMyThreadSafe();
               NANOS_INSTRUMENT( inst2.close() );

               total_spins = 0;
               total_yields = 0;
               time_yields = 0;

               sys.getSchedulerStats()._idleThreads++;
            } else {
               condition->unlock();
               if ( sys.useYield() ) {
                  total_yields++;
                  unsigned long begin_yield = (unsigned long) ( OS::getMonotonicTime() * 1.0e9  );
                  thread->yield();
                  unsigned long end_yield = (unsigned long) ( OS::getMonotonicTime() * 1.0e9  );
                  time_yields += ( end_yield - begin_yield );
               }
            }
         } else {
            condition->unlock();
         }
         spins = nspins;
      }

      thread->idle();
   }

   current->setSyncCond( NULL );
   sys.getSchedulerStats()._idleThreads--;
   if ( !current->isReady() ) {
      current->setReady();
   }

   total_spins+= (nspins - spins);
   NANOS_INSTRUMENT ( nanos_event_value_t Values[3]; )
   NANOS_INSTRUMENT ( Values[0] = (nanos_event_value_t) total_spins; )
   NANOS_INSTRUMENT ( Values[1] = (nanos_event_value_t) total_yields; )
   NANOS_INSTRUMENT ( Values[2] = (nanos_event_value_t) time_yields; )
   NANOS_INSTRUMENT( sys.getInstrumentation()->raisePointEventNkvs(3, Keys, Values); )
}

void Scheduler::wakeUp ( WD *wd )
{
   NANOS_INSTRUMENT( InstrumentState inst(NANOS_SYNCHRONIZATION) );
   if ( wd->isBlocked() ) {
      wd->setReady();
      Scheduler::queue(myThread, *wd );
   }
}

WD * Scheduler::prefetch( BaseThread *thread, WD &wd )
{
   return thread->getTeam()->getSchedulePolicy().atPrefetch( thread, wd );
}

struct WorkerBehaviour
{
   static WD * getWD ( BaseThread *thread, WD *current )
   {
      return thread->getTeam()->getSchedulePolicy().atIdle ( thread );
   }

   static void switchWD ( BaseThread *thread, WD *current, WD *next )
   {
      if (next->started())
        Scheduler::switchTo(next);
      else {
        Scheduler::inlineWork ( next );
      }
   }
};

void Scheduler::workerLoop ()
{
   idleLoop<WorkerBehaviour>();
}

void Scheduler::queue ( BaseThread *thread, WD &wd )
{
   sys.getSchedulerStats()._readyTasks++;
   thread->getTeam()->getSchedulePolicy().queue( thread, wd );
}

void Scheduler::inlineWork ( WD *wd )
{
   // run it in the current frame
   WD *oldwd = myThread->getCurrentWD();

   GenericSyncCond *syncCond = oldwd->getSyncCond();
   if ( syncCond != NULL ) {
      syncCond->unlock();
   }

   debug( "switching(inlined) from task " << oldwd << ":" << oldwd->getId() <<
          " to " << wd << ":" << wd->getId() );

   NANOS_INSTRUMENT( sys.getInstrumentation()->wdSwitch(oldwd, NULL, false) );

   // This ensures that when we return from the inlining is still the same thread
   // and we don't violate rules about tied WD
   wd->tieTo(*oldwd->isTiedTo());
   if (!wd->started())
      wd->init();
   myThread->setCurrentWD( *wd );

   NANOS_INSTRUMENT( sys.getInstrumentation()->wdSwitch( NULL, wd, false) );

   myThread->inlineWorkDependent(*wd);

   /* If WorkDescriptor has been submitted update statistics */
   updateExitStats (*wd);

   wd->done();

   NANOS_INSTRUMENT( sys.getInstrumentation()->wdSwitch(wd, NULL, false) );


   debug( "exiting task(inlined) " << wd << ":" << wd->getId() <<
          " to " << oldwd << ":" << oldwd->getId() );


   BaseThread *thread = getMyThreadSafe();
   thread->setCurrentWD( *oldwd );

   NANOS_INSTRUMENT( sys.getInstrumentation()->wdSwitch( NULL, oldwd, false) );

   // While we tie the inlined tasks this is not needed
   // as we will always return to the current thread
   #if 0
   if ( oldwd->isTiedTo() != NULL )
      switchToThread(oldwd->isTiedTo());
   #endif

   ensure(oldwd->isTiedTo() == NULL || thread == oldwd->isTiedTo(), 
           "Violating tied rules " + toString<BaseThread*>(thread) + "!=" + toString<BaseThread*>(oldwd->isTiedTo()));

}

void Scheduler::switchHelper (WD *oldWD, WD *newWD, void *arg)
{
   GenericSyncCond *syncCond = oldWD->getSyncCond();
   if ( syncCond != NULL ) {
      oldWD->setBlocked();
      syncCond->unlock();
   } else {
      Scheduler::queue( myThread, *oldWD );
   }

   NANOS_INSTRUMENT( sys.getInstrumentation()->wdSwitch(oldWD, NULL, false) );
   myThread->switchHelperDependent(oldWD, newWD, arg);

   myThread->setCurrentWD( *newWD );
   NANOS_INSTRUMENT( sys.getInstrumentation()->wdSwitch( NULL, newWD, false) );
}

void Scheduler::switchTo ( WD *to )
{
   if ( myThread->runningOn()->supportsUserLevelThreads() ) {
      if (!to->started()) {
         to->init();
         to->start(true);
      }
      
      debug( "switching from task " << myThread->getCurrentWD() << ":" << myThread->getCurrentWD()->getId() <<
          " to " << to << ":" << to->getId() );
          
      myThread->switchTo( to, switchHelper );
   } else {
      inlineWork(to);
      delete to;
   }
}

void Scheduler::yield ()
{
   NANOS_INSTRUMENT( InstrumentState inst(NANOS_SCHEDULING) );
   WD *next = myThread->getTeam()->getSchedulePolicy().atYield( myThread, myThread->getCurrentWD() );

   if ( next ) {
      sys.getSchedulerStats()._readyTasks--;
      switchTo(next);
   }
}

void Scheduler::switchToThread ( BaseThread *thread )
{
   while ( getMyThreadSafe() != thread )
        yield();
}

void Scheduler::exitHelper (WD *oldWD, WD *newWD, void *arg)
{
    myThread->exitHelperDependent(oldWD, newWD, arg);
    NANOS_INSTRUMENT ( sys.getInstrumentation()->wdSwitch(oldWD,newWD,true) );
    oldWD->~WorkDescriptor();
    delete[] (char *)oldWD;
    myThread->setCurrentWD( *newWD );
}

struct ExitBehaviour
{
   static WD * getWD ( BaseThread *thread, WD *current )
   {
      return thread->getTeam()->getSchedulePolicy().atExit( thread, current );
   }

   static void switchWD ( BaseThread *thread, WD *current, WD *next )
   {
      Scheduler::exitTo(next);
   }
};

void Scheduler::exitTo ( WD *to )
 {
//   FIXME: stack reusing was wrongly implementd and it's disabled (see #374)
//    WD *current = myThread->getCurrentWD();

    if (!to->started()) {
       to->init();
//       to->start(true,current);
       to->start(true,NULL);
    }

    debug( "exiting task " << myThread->getCurrentWD() << ":" << myThread->getCurrentWD()->getId() <<
          " to " << to << ":" << to->getId() );

    myThread->exitTo ( to, Scheduler::exitHelper );
}

void Scheduler::exit ( void )
{
   // At this point the WD work is done, so we mark it as such and look for other work to do
   // Deallocation doesn't happen here because:
   // a) We are still running in the WD stack
   // b) Resources can potentially be reused by the next WD

   WD *oldwd = myThread->getCurrentWD();

   updateExitStats (*oldwd);

   oldwd->done();
   oldwd->clear();

   idleLoop<ExitBehaviour>();

   fatal("A thread should never return from Scheduler::exit");
}

bool Scheduler::checkBasicConstraints ( WD &wd, BaseThread &thread )
{
   return wd.canRunIn(*thread.runningOn()) && ( !wd.isTied() || wd.isTiedTo() == &thread );
}
