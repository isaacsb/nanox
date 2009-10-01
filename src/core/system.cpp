#include "system.hpp"
#include "config.hpp"
#include "plugin.hpp"
#include "schedule.hpp"

using namespace nanos;

System nanos::sys;

//cutoff * createDummyCutoff();
//class dummy_cutoff;
//cutoff * createLevelCutoff();
//class level_cutoff;
//cutoff * createTasknumCutoff();
//class tasknum_cutoff;
//cutoff * createIdleCutoff();
//class idle_cutoff;
//cutoff * createReadyCutoff();
//class ready_cutoff;



// default system values go here
 System::System () : numPEs(1), binding(true), profile(false), instrument(false),
                     verboseMode(false), executionMode(DEDICATED), thsPerPE(1),
                     defSchedule("cilk"), defCutoff("tasknum")
{
    //cutOffPolicy = createTasknumCutoff();
    verbose0 ( "NANOS++ initalizing... start" );
    config();   
    loadModules();
    start();
    verbose0 ( "NANOS++ initalizing... end" );
}

void System::loadModules ()
{
   verbose0 ( "Loading modules" );

   // load host processor module
   verbose0("loading SMP support");
   if ( !PluginManager::load ( "pe-smp" ) )
      fatal0 ( "Couldn't load SMP support" );
   ensure(hostFactory,"No default smp factory");

   // load default schedule plugin
   verbose0("loading " << getDefaultSchedule() << " scheduling policy support");
   if ( !PluginManager::load ( "sched-"+getDefaultSchedule() ) )
      fatal0 ( "Couldn't load main scheduling policy" );

   ensure(defSGFactory,"No default system scheduling factory");

   verbose0( "loading task num cutoff policy" );
   if( !PluginManager::load( "cutoff-"+getDefaultCutoff() ) )
      fatal0( "Could not load main cutoff policy" );
}


void System::config ()
{
   Config config;

   verbose0 ( "Preparing configuration" );
   config.registerArgOption(new Config::PositiveVar("nth-pes",numPEs));
   config.registerEnvOption(new Config::PositiveVar("NTH_PES",numPEs));
   config.registerArgOption(new Config::FlagOption("nth-bindig",binding));
   config.registerArgOption(new Config::FlagOption("nth-verbose",verboseMode));

   //more than 1 thread per pe
   config.registerArgOption(new Config::PositiveVar("nth-thsperpe",thsPerPE));

    //TODO: how to simplify this a bit?
   Config::MapVar<ExecutionMode>::MapList opts(2);
   opts[0] = Config::MapVar<ExecutionMode>::MapOption("dedicated",DEDICATED);
   opts[1] = Config::MapVar<ExecutionMode>::MapOption("shared",SHARED);
   config.registerArgOption(
                            new Config::MapVar<ExecutionMode>("nth-mode",executionMode,opts));

   config.registerArgOption ( new Config::StringVar ( "nth-schedule", defSchedule ) );
   config.registerEnvOption ( new Config::StringVar ( "NTH_SCHEDULE", defSchedule ) );

   config.registerArgOption ( new Config::StringVar ( "nth-cutoff", defCutoff ) );
   config.registerEnvOption ( new Config::StringVar ( "NTH_CUTOFF", defCutoff ) );

   verbose0 ( "Reading Configuration" );
   config.init();
}

PE * System::createPE ( std::string pe_type, int pid )
{
   // TODO: lookup table for PE factories
   // in the mean time assume only one factory

   return hostFactory(pid);
}

void System::start ()
{
   verbose0 ( "Starting threads" );

    int numPes = getNumPEs();

   pes.reserve ( numPes );

   SchedulingGroup *sg = defSGFactory();

   //TODO: decide, single master, multiple master start
   PE *pe = createPE ( "smp", 0 );
   pes.push_back ( pe );
   pe->associateThisThread ( sg );

    //starting as much threads per pe as requested by the user
    for(int ths = 1; ths < getThsPerPE(); ths++) {
         pe->startWorker(sg);
     }

    for ( int p = 1; p < numPes ; p++ ) {
      pe = createPE ( "smp", p );
      pes.push_back ( pe );

      //starting as much threads per pe as requested by the user
      for(int ths = 0; ths < getThsPerPE(); ths++) {
         pe->startWorker(sg);
      }
   }
}

System::~System ()
{
   verbose ( "NANOS++ shutting down.... init" );

   verbose ( "Wait for main workgroup to complete" );
   myThread->getCurrentWD()->waitCompletation();

   verbose ( "Joining threads... phase 1" );
   // signal stop PEs

   for ( unsigned p = 1; p < pes.size() ; p++ ) {
      pes[p]->stopAll();
   }

   verbose ( "Joining threads... phase 2" );

   // join

   for ( unsigned p = 1; p < pes.size() ; p++ ) {
      delete pes[p];
   }

   verbose ( "NANOS++ shutting down.... end" );
}

void System::submit ( WD &work )
{
   work.setParent ( myThread->getCurrentWD() );
   work.setLevel( work.getParent()->getLevel() +1 );
   Scheduler::submit ( work );
}


bool System::throttleTask() {
  return cutOffPolicy->cutoff_pred();
}


