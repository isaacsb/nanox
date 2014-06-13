#include "plugin.hpp"
#include "system.hpp"
#include "instrumentation.hpp"
#include "instrumentationcontext_decl.hpp"
#include "ompt_callbacks.h"

extern "C" {

   int ompt_initialize(
         ompt_function_lookup_t lookup,  /* function to look up OMPT API routines by name */
         const char *runtime_version,    /* OpenMP runtime version string */
         unsigned int ompt_version       /* integer that identifies the OMPT revision */
         );

   int ompt_initialize( ompt_function_lookup_t lookup, const char *runtime_version, unsigned int ompt_version )
   {
      fatal( "There is no OMPT compliant tool loaded\n" );
      return 0;
   } 

   #define OMPT_NANOS_STATES 5
   ompt_state_t nanos_state_values[OMPT_NANOS_STATES] = { ompt_state_first, ompt_state_idle, ompt_state_work_serial, ompt_state_work_parallel, ompt_state_undefined };
   const char  *nanos_state_string[OMPT_NANOS_STATES] = { "First", "Idle", "Serial", "Parallel", "Undefined" };

   //! List of callback declarations
   ompt_new_parallel_callback_t  ompt_nanos_event_parallel_begin = NULL;
   ompt_parallel_callback_t      ompt_nanos_event_parallel_end = NULL;
   ompt_new_task_callback_t      ompt_nanos_event_task_begin = NULL;
   ompt_task_callback_t          ompt_nanos_event_task_end = NULL;
   ompt_thread_type_callback_t   ompt_nanos_event_thread_begin = NULL;
   ompt_thread_type_callback_t   ompt_nanos_event_thread_end = NULL;
   ompt_control_callback_t       ompt_nanos_event_control = NULL;
   ompt_callback_t               ompt_nanos_event_shutdown = NULL;

   int ompt_nanos_set_callback( ompt_event_t event, ompt_callback_t callback );
   int ompt_nanos_set_callback( ompt_event_t event, ompt_callback_t callback )
   {
      // Return values:
      // 0 callback registration error (e.g., callbacks cannot be registered at this time).
      // 1 event may occur; no callback is possible
      // 2 event will never occur in runtime
      // 3 event may occur; callback invoked when convenient
      // 4 event may occur; callback always invoked when event occurs

      switch ( event ) {
         case ompt_event_parallel_begin:
            ompt_nanos_event_parallel_begin = (ompt_new_parallel_callback_t) callback;
            return 4;
         case ompt_event_parallel_end:
            ompt_nanos_event_parallel_end = (ompt_parallel_callback_t) callback;
            return 4;
         case ompt_event_task_begin: 
            ompt_nanos_event_task_begin = (ompt_new_task_callback_t) callback;
            return 4;
         case ompt_event_task_end:
            ompt_nanos_event_task_end = (ompt_task_callback_t) callback;
            return 4;
         case ompt_event_thread_begin:
            ompt_nanos_event_thread_begin = (ompt_thread_type_callback_t) callback;
            return 4;
         case ompt_event_thread_end:
            ompt_nanos_event_thread_end = (ompt_thread_type_callback_t) callback;
            return 4;
         case ompt_event_control:
            //FIXME Consider to instrument user control calls
            ompt_nanos_event_control = (ompt_control_callback_t) callback;
            return 1;
         case ompt_event_runtime_shutdown:
            ompt_nanos_event_shutdown = (ompt_callback_t) callback;
            return 4;
         case ompt_event_idle_begin:
         case ompt_event_idle_end:
         case ompt_event_wait_barrier_begin:
         case ompt_event_wait_barrier_end:
         case ompt_event_wait_taskwait_begin:
         case ompt_event_wait_taskwait_end:
         case ompt_event_wait_taskgroup_begin:
         case ompt_event_wait_taskgroup_end:
         case ompt_event_release_lock:
         case ompt_event_release_nest_lock_last:
         case ompt_event_release_critical:
         case ompt_event_release_atomic:
         case ompt_event_release_ordered:
         case ompt_event_implicit_task_begin:
         case ompt_event_implicit_task_end:
         case ompt_event_initial_task_begin:
         case ompt_event_initial_task_end:
         case ompt_event_task_switch:
         case ompt_event_loop_begin:
         case ompt_event_loop_end:
         case ompt_event_sections_begin:
         case ompt_event_sections_end:
         case ompt_event_single_in_block_begin:
         case ompt_event_single_in_block_end:
         case ompt_event_single_others_begin:
         case ompt_event_single_others_end:
         case ompt_event_workshare_begin:
         case ompt_event_workshare_end:
         case ompt_event_master_begin:
         case ompt_event_master_end:
         case ompt_event_barrier_begin:
         case ompt_event_barrier_end:
         case ompt_event_taskwait_begin:
         case ompt_event_taskwait_end:
         case ompt_event_taskgroup_begin:
         case ompt_event_taskgroup_end:
         case ompt_event_release_nest_lock_prev:
         case ompt_event_wait_lock:
         case ompt_event_wait_nest_lock:
         case ompt_event_wait_critical:
         case ompt_event_wait_atomic:
         case ompt_event_wait_ordered:
         case ompt_event_acquired_lock:
         case ompt_event_acquired_nest_lock_first:
         case ompt_event_acquired_nest_lock_next:
         case ompt_event_acquired_critical:
         case ompt_event_acquired_atomic:
         case ompt_event_acquired_ordered:
            return 1;
         default:
            warning("Callback registration error");
            return 0;
      }

   }

   int ompt_nanos_get_callback( ompt_event_t event, ompt_callback_t *callback );
   int ompt_nanos_get_callback( ompt_event_t event, ompt_callback_t *callback )
   {
      // FIXME: TBD
      return 0;
   }

   int ompt_nanos_enumerate_state( ompt_state_t current_state, ompt_state_t *next_state, const char **next_state_name );
   int ompt_nanos_enumerate_state( ompt_state_t current_state, ompt_state_t *next_state, const char **next_state_name )
   {
      int i;

      for ( i = 0; i < OMPT_NANOS_STATES; i++) {
         if ( nanos_state_values[i] == current_state ) break;
      }

      if ( ++i < OMPT_NANOS_STATES ) {
         *next_state = nanos_state_values[i];
         *next_state_name = nanos_state_string[i];
         return 1;
      } else {
         return 0;
      }
   }

   ompt_thread_id_t ompt_nanos_get_thread_id( void );
   ompt_thread_id_t ompt_nanos_get_thread_id( void )
   {
      return (ompt_thread_id_t) nanos::myThread->getId();
      
   }

   ompt_state_t ompt_nanos_get_state( ompt_wait_id_t *wait_id );
   ompt_state_t ompt_nanos_get_state( ompt_wait_id_t *wait_id )
   {
      //! \note This function must be ordered acording with state genericity
      //! specific states must be first to be detected, so the first state that fits
      //! with curret thread state must be returned.


      //! \note If thread state is idle, return idle
      if ( myThread->isIdle() ) return ompt_state_idle;

      //! \note If we consider OmpSs running always in parallel there is only when
      //! case in which we can run serially: first level team, running with one thread. 
      ThreadTeam *tt = myThread->getTeam();
      if ( tt && tt->size() == 1 && tt->getLevel() == 0 ) 
         return ompt_state_work_serial;
      else if ( tt )
         return ompt_state_work_parallel;

      //! \note Otherwise return undefined
      return ompt_state_undefined;
   }

   void * ompt_nanos_get_idle_frame(void);
   void * ompt_nanos_get_idle_frame(void)
   {
      // FIXME: TBD
      return NULL;
   }

   ompt_parallel_id_t ompt_nanos_get_parallel_id( int ancestor_level );
   ompt_parallel_id_t ompt_nanos_get_parallel_id( int ancestor_level )
   {
      // FIXME: TBD
      return (ompt_parallel_id_t) 0;
   }

   int ompt_nanos_get_parallel_team_size( int ancestor_level );
   int ompt_nanos_get_parallel_team_size( int ancestor_level )
   {
      // FIXME: TBD
      return (int) 0;
   }

   // XXX: Is the return value actually a pointer to task_id_t
   ompt_task_id_t *ompt_nanos_get_task_id( int depth );
   ompt_task_id_t *ompt_nanos_get_task_id( int depth )
   {
      // FIXME: TBD
      return NULL;
   }

   ompt_frame_t *ompt_nanos_get_task_frame( int depth );
   ompt_frame_t *ompt_nanos_get_task_frame( int depth )
   {
      // FIXME: TBD
      return NULL;
   }

   ompt_interface_fn_t ompt_nanos_lookup ( const char *entry_point );
   ompt_interface_fn_t ompt_nanos_lookup ( const char *entry_point )
   {
      if ( strncmp( entry_point, "ompt_set_callback", strlen("ompt_set_callback") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_set_callback;
      if ( strncmp( entry_point, "ompt_get_callback", strlen("ompt_get_callback") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_callback;
      if ( strncmp( entry_point, "ompt_enumerate_state", strlen("ompt_enumerate_state") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_enumerate_state;
      if ( strncmp( entry_point, "ompt_get_thread_id", strlen("ompt_get_thread_id") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_thread_id;
      if ( strncmp( entry_point, "ompt_get_state", strlen("ompt_get_state") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_state;
      if ( strncmp( entry_point, "ompt_get_idle_frame", strlen("ompt_get_idle_frame") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_idle_frame;
      if ( strncmp( entry_point, "ompt_get_parallel_id", strlen("ompt_get_parallel_id") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_parallel_id;
      if ( strncmp( entry_point, "ompt_get_parallel_team_size", strlen("ompt_get_parallel_team_size") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_parallel_team_size;
      if ( strncmp( entry_point, "ompt_get_task_id", strlen("ompt_get_task_id") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_task_id;
      if ( strncmp( entry_point, "ompt_get_task_frame", strlen("ompt_get_task_frame") ) == 0 )
         return ( ompt_interface_fn_t ) ompt_nanos_get_task_frame;
 
      return (NULL);
   }
}

namespace nanos
{
   class InstrumentationOMPT: public Instrumentation 
   {
      public:
         InstrumentationOMPT( ) : Instrumentation( *NEW InstrumentationContextDisabled() ) {}
         ~InstrumentationOMPT() { }
         void initialize( void )
         {
            ompt_initialize ( ompt_nanos_lookup, "Nanos++ 0.8a", 1);
         }
         void finalize( void ) { if ( ompt_nanos_event_shutdown ) ompt_nanos_event_shutdown(); }
         void disable( void ) {}
         void enable( void ) {}
         void addEventList ( unsigned int count, Event *events )
         {
            InstrumentationDictionary *iD = getInstrumentationDictionary();
            static const nanos_event_key_t create_wd_ptr = iD->getEventKey("create-wd-ptr");
            static const nanos_event_key_t api = iD->getEventKey("api");
            static const nanos_event_value_t api_enter_team = iD->getEventValue("api","enter_team");
            static const nanos_event_value_t api_leave_team = iD->getEventValue("api","leave_team");

            unsigned int i;
            for( i=0; i<count; i++) {
               Event &e = events[i];
               if ( e.getKey( ) == create_wd_ptr && ompt_nanos_event_task_begin )
               { 
                  ompt_nanos_event_task_begin(
                     (ompt_task_id_t) nanos::myThread->getCurrentWD()->getId(),
                     NULL,  // FIXME: task frame
                     (ompt_task_id_t) ((WorkDescriptor *)e.getValue())->getId(),
                     NULL   // FIXME: outlined function
                  );
               }
               else if ( e.getKey( ) == api )
               {
                  nanos_event_value_t val = e.getValue();
                
                  if ( val == api_enter_team && ompt_nanos_event_parallel_begin )
                  {
                     ompt_nanos_event_parallel_begin (
                        (ompt_task_id_t) nanos::myThread->getCurrentWD()->getId(),
                        (ompt_frame_t) NULL,    // FIXME: frame data of parent task
                        (ompt_parallel_id_t) 0, // FIXME: parallel_id
                        (uint32_t) 0,           // FIXME: requested team size
                        (void *) NULL           // FIXME: outlined function
                     );
                  }
                  else if ( val == api_leave_team && ompt_nanos_event_parallel_end )
                  {
                     ompt_nanos_event_parallel_end (
                           (ompt_parallel_id_t) 0, // FIXME: parallel_id
                           (ompt_task_id_t) nanos::myThread->getCurrentWD()->getId() );
                  }
          
               }
            }
         }
         void addResumeTask( WorkDescriptor &w ) {}
         void addSuspendTask( WorkDescriptor &w, bool last )
         {
            if (ompt_nanos_event_task_end && last) ompt_nanos_event_task_end((ompt_task_id_t) w.getId());
         }
         void threadStart( BaseThread &thread ) 
         {
            if (ompt_nanos_event_thread_begin) {
               ompt_nanos_event_thread_begin( (ompt_thread_type_t) ompt_thread_worker, (ompt_thread_id_t) nanos::myThread->getId());
            }
         }
         void threadFinish ( BaseThread &thread )
         {
            if (ompt_nanos_event_thread_end) {
               ompt_nanos_event_thread_end((ompt_thread_type_t) ompt_thread_worker, (ompt_thread_id_t) nanos::myThread->getId());
            }
         }
         void incrementMaxThreads( void ) {}
   };
   namespace ext
   {
      class InstrumentationOMPTPlugin : public Plugin
      {
         public:
            InstrumentationOMPTPlugin () : Plugin("Instrumentation OMPT compatible.",1) {}
            ~InstrumentationOMPTPlugin () {}
            void config( Config &cfg ) {}
            void init () { sys.setInstrumentation( NEW InstrumentationOMPT() ); }
      };
   } // namespace ext
} // namespace nanos

DECLARE_PLUGIN("instrumentation-ompt",nanos::ext::InstrumentationOMPTPlugin);
