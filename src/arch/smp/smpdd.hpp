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

#ifndef _NANOS_SMP_WD
#define _NANOS_SMP_WD

#include <stdint.h>
#include "workdescriptor.hpp"
#include "config.hpp"

namespace nanos {
namespace ext
{

   extern Device SMP;

   class SMPDD : public DD
   {

      public:
         typedef void ( *work_fct ) ( void *self );

      private:
         work_fct       _work;
         intptr_t *     _stack;
         intptr_t *     _state;
         static int     _stackSize;

         void initStackDep ( void *userf, void *data, void *cleanup );

      public:
         // constructors
         SMPDD( work_fct w ) : DD( &SMP ),_work( w ),_stack( 0 ),_state( 0 ) {}

         SMPDD() : DD( &SMP ),_work( 0 ),_stack( 0 ),_state( 0 ) {}

         // copy constructors
         SMPDD( const SMPDD &dd ) : DD( dd ), _work( dd._work ), _stack( 0 ), _state( 0 ) {}

         // assignment operator
         const SMPDD & operator= ( const SMPDD &wd );
         // destructor

         virtual ~SMPDD() { if ( _stack ) delete[] _stack; }

         work_fct getWorkFct() const { return _work; }

         bool hasStack() { return _state != NULL; }

         void allocateStack();
         void initStack( void *data );

         intptr_t *getState() const { return _state; }

         void setState ( intptr_t * newState ) { _state = newState; }

         static void prepareConfig( Config &config );
   };

   inline const SMPDD & SMPDD::operator= ( const SMPDD &dd )
   {
      // self-assignment: ok
      if ( &dd == this ) return *this;

      DD::operator= ( dd );

      _work = dd._work;
      _stack = 0;
      _state = 0;

      return *this;
   }

}
}

#endif