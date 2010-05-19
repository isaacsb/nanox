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
#ifndef __NANOS_INSTRUMENTOR_H
#define __NANOS_INSTRUMENTOR_H

#include "instrumentor_decl.hpp"

using namespace nanos;

inline nanos_event_value_t InstrumentorValueDescriptor::getId ( void )
{
   return _id;
}

inline const std::string InstrumentorValueDescriptor::getDescription ( void )
{
   return _description;
}


/** INSTRUMENTOR KEY DESCRIPTOR **/

inline nanos_event_key_t InstrumentorKeyDescriptor::getId ( void )
{
   return _id;
}

inline const std::string InstrumentorKeyDescriptor::getDescription ( void )
{
   return _description;
}

inline nanos_event_value_t InstrumentorKeyDescriptor::registerValue ( const std::string &value, const std::string &description,
                                                                      bool abort_when_registered )
{
   return registerValue( value.c_str(), description.c_str(), abort_when_registered );
}

inline nanos_event_value_t InstrumentorKeyDescriptor::registerValue ( const char *value, const char *description, bool abort_when_registered )
{
   InstrumentorValueDescriptor *valueDescriptor = NULL;

   ValueMapIterator it = _valueMap.find( value );

   if ( it == _valueMap.end() ) {
      _lock++;
      it = _valueMap.find( value );
      if ( it == _valueMap.end() ) {
         valueDescriptor = new InstrumentorValueDescriptor ( (nanos_event_value_t) _totalValues++, description );
         _valueMap.insert( std::make_pair( value, valueDescriptor ) );
      }
      else {
         if ( abort_when_registered ) fatal0("Event Value was already registered (lock taken)\n");
         valueDescriptor = it->second;
      }
      _lock--;
   }
   else {
      if ( abort_when_registered ) fatal0("Event Value was already registered (lock not taken)\n");
      valueDescriptor = it->second;
   }

   return valueDescriptor->getId();
}

inline nanos_event_value_t InstrumentorKeyDescriptor::getValue ( const std::string &value )
{
   return getValue( value.c_str() );
}

inline nanos_event_value_t InstrumentorKeyDescriptor::getValue ( const char *value )
{
   ValueMapIterator it = _valueMap.find( value );

   if ( it == _valueMap.end() ) return (nanos_event_value_t) 0; 
   else return it->second->getId();
}

inline InstrumentorKeyDescriptor::ConstValueMapIterator InstrumentorKeyDescriptor::beginValueMap ( void )
{
   return _valueMap.begin();
}

inline InstrumentorKeyDescriptor::ConstValueMapIterator InstrumentorKeyDescriptor::endValueMap ( void )
{
   return _valueMap.end();
}

/** INSTRUMENTOR DICTIONARY **/

inline nanos_event_key_t InstrumentorDictionary::registerEventKey ( const std::string &key, const std::string &description, bool abort_when_registered  )
{
   return registerEventKey( key.c_str(), description.c_str(), abort_when_registered );
}

inline nanos_event_key_t InstrumentorDictionary::registerEventKey ( const char *key, const char *description, bool abort_when_registered  )
{
   InstrumentorKeyDescriptor *keyDescriptor = NULL;

   KeyMapIterator it = _keyMap.find( key );

   if ( it == _keyMap.end() ) {
      _lock++;
      it = _keyMap.find( key );
      if ( it == _keyMap.end() ) {
         keyDescriptor = new InstrumentorKeyDescriptor ( (nanos_event_key_t) _totalKeys++, description );
         _keyMap.insert( std::make_pair( key, keyDescriptor ) );
      }
      else {
         if ( abort_when_registered ) fatal0("Event Key was already registered (lock taken)\n");
         keyDescriptor = it->second;
      }
      _lock--;
   }
   else {
      if ( abort_when_registered ) fatal0("Event Key was already registered (lock not taken)\n");
      keyDescriptor = it->second;
   }

   return keyDescriptor->getId();
}

inline nanos_event_key_t InstrumentorDictionary::getEventKey ( const std::string &key )
{
   return getEventKey( key.c_str() );
}

inline nanos_event_key_t InstrumentorDictionary::getEventKey ( const char *key )
{
   KeyMapIterator it = _keyMap.find( key );

   if ( it == _keyMap.end() ) return (nanos_event_key_t) 0;
   else return it->second->getId();
}

inline nanos_event_value_t InstrumentorDictionary::registerEventValue ( const std::string &key, const std::string &value, const std::string &description, bool abort_when_registered )
{
   return registerEventValue ( key.c_str(), value.c_str(), description.c_str(), abort_when_registered );
}

inline nanos_event_value_t InstrumentorDictionary::registerEventValue ( const char *key, const char *value, const char *description, bool abort_when_registered )
{
   InstrumentorKeyDescriptor *keyDescriptor = NULL;

   KeyMapIterator it = _keyMap.find( key );

   if ( it == _keyMap.end() ) {
      _lock++;
      it = _keyMap.find( key );
      if ( it == _keyMap.end() ) {
         keyDescriptor = new InstrumentorKeyDescriptor ( (nanos_event_key_t) _totalKeys++, "" );
         _keyMap.insert( std::make_pair( key, keyDescriptor ) );
      }
      else {
         keyDescriptor = it->second;
      }
      _lock--;
   }
   else keyDescriptor = it->second;

   return keyDescriptor->registerValue( value, description, abort_when_registered );
}

inline nanos_event_value_t InstrumentorDictionary::getEventValue ( const std::string &key, const std::string &value )
{
   return getEventValue ( key.c_str(), value.c_str() );
}

inline nanos_event_value_t InstrumentorDictionary::getEventValue ( const char *key, const char *value )
{
   KeyMapIterator it = _keyMap.find( key );

   if ( it == _keyMap.end() ) return (nanos_event_value_t) 0;
   else return it->second->getValue(value);
}

inline InstrumentorDictionary::ConstKeyMapIterator InstrumentorDictionary::beginKeyMap ( void )
{
   return _keyMap.begin();
}

inline InstrumentorDictionary::ConstKeyMapIterator InstrumentorDictionary::endKeyMap ( void )
{
   return _keyMap.end();
}

/** INSTRUMENTOR **/

inline InstrumentorDictionary * Instrumentor::getInstrumentorDictionary ( void ) { return &_instrumentorDictionary; }

inline nanos_event_type_t Instrumentor::Event::getType () const { return _type; }

inline nanos_event_state_value_t Instrumentor::Event::getState () { return _state; }

inline unsigned int Instrumentor::Event::getNumKVs () const { return _nkvs; }

inline Instrumentor::Event::ConstKVList Instrumentor::Event::getKVs () const { return _kvList; }

inline unsigned int Instrumentor::Event::getDomain ( void ) const { return _ptpDomain; }

inline unsigned int Instrumentor::Event::getId( void ) const { return _ptpId; }

inline void Instrumentor::Event::reverseType ( )
{
   switch ( _type )
   {
      case PTP_START: _type = PTP_END; break;
      case PTP_END: _type = PTP_START; break;
      case BURST_START: _type = BURST_END; break;
      case BURST_END: _type = BURST_START; break;
      default: break;
   }
}

#endif
