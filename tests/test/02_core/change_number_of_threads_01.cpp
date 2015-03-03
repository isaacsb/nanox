#include <stdio.h>
#include "system.hpp"

/*
<testinfo>
   test_generator="gens/mixed-generator -a --no-warmup-threads|--warmup-threads"
   test_generator_ENV=( "NX_TEST_MAX_CPUS=1"
                        "NX_TEST_SCHEDULE=bf"
                        "test_architecture=smp")
   test_ignore_fail=1
</testinfo>
*/

#define NTHREADS_PHASE_1 1
#define NTHREADS_PHASE_2 2

int main ( int argc, char *argv[])
{
   int error = 0;

   fprintf(stdout,"Thread team final size is %d and %d is expected\n",
      (int) myThread->getTeam()->getFinalSize(),
      (int) NTHREADS_PHASE_1
   );
   if ( myThread->getTeam()->getFinalSize() != NTHREADS_PHASE_1 ) error++;

   sys.updateActiveWorkers( NTHREADS_PHASE_2 );

   fprintf(stdout,"Thread team final size is %d and %d is expected\n",
      (int) myThread->getTeam()->getFinalSize(),
            NTHREADS_PHASE_2
   );
   if ( myThread->getTeam()->getFinalSize() != NTHREADS_PHASE_2 ) error++;

   sys.updateActiveWorkers( NTHREADS_PHASE_1 );

   fprintf(stdout,"Thread team final size is %d and %d is expected\n",
      (int) myThread->getTeam()->getFinalSize(),
      (int) NTHREADS_PHASE_1
   );
   if ( myThread->getTeam()->getFinalSize() != NTHREADS_PHASE_1 ) error++;

   fprintf(stdout,"Result is %s\n", error? "UNSUCCESSFUL":"successful");

   return error;
}

