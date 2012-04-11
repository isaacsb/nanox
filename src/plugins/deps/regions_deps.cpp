/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

#include "basedependenciesdomain.hpp"
#include "plugin.hpp"
#include "system.hpp"
#include "config.hpp"
#include "compatibility.hpp"
#include "regiontree.hpp"
#include "regionbuilder.hpp"
#include "trackableobject.hpp"

namespace nanos {
   namespace ext {

      class RegionDependenciesDomain : public BaseDependenciesDomain
      {
         private:
            typedef RegionTree<TrackableObject> RegionMap; /**< Maps regions to \a RegionStatus objects */
            
         private:
            RegionMap            _regionMap;            /**< Used to track dependencies between DependableObject */
         private:
            
         protected:
            /*! \brief Assigns the DependableObject depObj an id in this domain and adds it to the domains dependency system.
             *  \param depObj DependableObject to be added to the domain.
             *  \param begin Iterator to the start of the list of dependencies to be associated to the Dependable Object.
             *  \param end Iterator to the end of the mentioned list.
             *  \param callback A function to call when a WD has a successor [Optional].
             *  \sa Dependency DependableObject TrackableObject
             */
            template<typename const_iterator>
            void submitDependableObjectInternal ( DependableObject &depObj, const_iterator begin, const_iterator end, SchedulePolicySuccessorFunctor* callback )
            {
               depObj.setId ( _lastDepObjId++ );
               depObj.init();
               depObj.setDependenciesDomain( this );
            
               // Object is not ready to get its dependencies satisfied
               // so we increase the number of predecessors to permit other dependableObjects to free some of
               // its dependencies without triggering the "dependenciesSatisfied" method
               depObj.increasePredecessors();
            
               // This tree is used for coalescing the accesses to avoid duplicates
               typedef RegionTree<AccessType> access_type_region_tree_t;
               typedef access_type_region_tree_t::iterator_list_t::iterator access_type_accessor_iterator_t;
               access_type_region_tree_t accessTypeRegionTree;
               for ( const_iterator it = begin; it != end; it++ ) {
                  DataAccess const &dataAccess = *it;
                  
                  // Find out the displacement due to the lower bounds and correct it in the address
                  size_t base = 1UL;
                  /*size_t displacement = 0L;
                  for (short dimension = 0; dimension < dataAccess.dimension_count; dimension++) {
                     nanos_region_dimension_internal_t const &dimensionData = dataAccess.dimensions[dimension];
                     displacement = displacement + dimensionData.lower_bound * base;
                     base = base * dimensionData.size;
                  }
                  size_t address = (size_t)dataAccess.address + displacement;*/
                  size_t address = (size_t)dataAccess.address + dataAccess.offset;
                  
                  // Build the Region
                  
                  // First dimension is base 1
                  size_t additionalContribution = 0UL; // Contribution of the previous dimensions (necessary due to alignment issues)
                  Region region = RegionBuilder::build(address, 1UL, dataAccess.dimensions[0].accessed_length, additionalContribution);
                  
                  // Add the bits corresponding to the rest of the dimensions (base the previous one)
                  base = 1 * dataAccess.dimensions[0].size;
                  for (short dimension = 1; dimension < dataAccess.dimension_count; dimension++) {
                     nanos_region_dimension_internal_t const &dimensionData = dataAccess.dimensions[dimension];
                     
                     region |= RegionBuilder::build(address, base, dimensionData.accessed_length, additionalContribution);
                     base = base * dimensionData.size;
                  }
                  
                  typename access_type_region_tree_t::iterator_list_t accessors;
                  access_type_region_tree_t::iterator exactAccessor = accessTypeRegionTree.findAndPopulate(region, accessors);
                  if (!exactAccessor.isEmpty()) {
                     accessors.push_back(exactAccessor);
                  }
                  for (access_type_accessor_iterator_t it2 = accessors.begin(); it2 != accessors.end(); it2++) {
                     access_type_region_tree_t::iterator accessor = *it2;
                     (*accessor) |= dataAccess.flags;
                  }
               }
               
               // This list is needed for waiting
               std::list<uint64_t> flushDeps;
               
               {
                  typename access_type_region_tree_t::iterator_list_t accessors;
                  accessTypeRegionTree.find ( Region(0UL, 0UL), accessors );
                  for (access_type_accessor_iterator_t it = accessors.begin(); it != accessors.end(); it++) {
                     access_type_region_tree_t::iterator accessor = *it;
                     Region region = accessor.getRegion();
                     AccessType const &accessType = *accessor;
                     
                     submitDependableObjectDataAccess( depObj, region, accessType, callback );
                     flushDeps.push_back( (uint64_t) region.getFirstValue() );
                  }
               }
                  
               // To keep the count consistent we have to increase the number of tasks in the graph before releasing the fake dependency
               increaseTasksInGraph();
            
               depObj.submitted();
            
               // now everything is ready
               if ( depObj.decreasePredecessors() > 0 )
                  depObj.wait( flushDeps );
            }
            
            /*! \brief Adds a region access of a DependableObject to the domains dependency system.
             *  \param depObj target DependableObject
             *  \param target accessed memory address
             *  \param accessType kind of region access
             *  \param callback Function to call if an immediate predecessor is found.
             */
            void submitDependableObjectDataAccess( DependableObject &depObj, Region const &target, AccessType const &accessType, SchedulePolicySuccessorFunctor* callback )
            {
               if ( accessType.commutative ) {
                  if ( !( accessType.input && accessType.output ) || depObj.waits() ) {
                     fatal( "Commutation task must be inout" );
                  }
               }
               
               SyncRecursiveLockBlock lock1( getInstanceLock() );
               //typedef std::set<RegionMap::iterator> subregion_set_t;
               typedef RegionMap::iterator_list_t subregion_set_t;
               subregion_set_t subregions;
               RegionMap::iterator wholeRegion = _regionMap.findAndPopulate( target, /* out */subregions );
               if ( !wholeRegion.isEmpty() ) {
                  subregions.push_back(wholeRegion);
               }
               
               for (
                  subregion_set_t::iterator it = subregions.begin();
                  it != subregions.end();
                  it++
               ) {
                  RegionMap::iterator &accessor = *it;
                  TrackableObject &status = *accessor;
                  status.hold(); // This is necessary since we may trigger a removal in finalizeReduction
               }
               
               for (
                  subregion_set_t::iterator it = subregions.begin();
                  it != subregions.end();
                  it++
               ) {
                  RegionMap::iterator &accessor = *it;
                  
                  Region const &subregion = accessor.getRegion();
                  TrackableObject &status = *accessor;
                  
                  if ( accessType.commutative ) {
                     submitDependableObjectCommutativeDataAccess( depObj, subregion, accessType, status, callback );
                  } else if ( accessType.input && accessType.output ) {
                     submitDependableObjectInoutDataAccess( depObj, subregion, accessType, status, callback );
                  } else if ( accessType.input ) {
                     submitDependableObjectInputDataAccess( depObj, subregion, accessType, status, callback );
                  } else if ( accessType.output ) {
                     submitDependableObjectOutputDataAccess( depObj, subregion, accessType, status, callback );
                  } else {
                     fatal( "Invalid dara access" );
                  }
                  
                  status.unhold();
                  // Just in case
                  if ( status.isEmpty() ) {
                     accessor.erase();
                  }
               }
               
               if ( !depObj.waits() && !accessType.commutative ) {
                  if ( accessType.output ) {
                     depObj.addWriteTarget( target );
                  } else if (accessType.input /* && !accessType.output && !accessType.commutative */ ) {
                     depObj.addReadTarget( target );
                  }
               }
            }
            
            void deleteLastWriter ( DependableObject &depObj, BaseDependency const &target )
            {
               const Region& region( dynamic_cast<const Region&>( target ) );
               
               SyncRecursiveLockBlock lock1( getInstanceLock() );
               RegionMap::iterator_list_t subregions;
               _regionMap.find( region, /* out */subregions );
               
               for (
                  RegionMap::iterator_list_t::iterator it = subregions.begin();
                  it != subregions.end();
                  it++
               ) {
                  RegionMap::iterator &accessor = *it;
                  TrackableObject &status = *accessor;
                  
                  status.deleteLastWriter(depObj);
                  
                  if ( status.isEmpty() && !status.isOnHold( ) ) {
                     accessor.erase( );
                  }
               }
            }
            
            
            void deleteReader ( DependableObject &depObj, BaseDependency const &target )
            {
               const Region& region( dynamic_cast<const Region&>( target ) );
            
               SyncRecursiveLockBlock lock1( getInstanceLock() );
               RegionMap::iterator_list_t subregions;
               _regionMap.find( region, /* out */subregions );
               
               for (
                  RegionMap::iterator_list_t::iterator it = subregions.begin();
                  it != subregions.end();
                  it++
               ) {
                  RegionMap::iterator &accessor = *it;
                  TrackableObject &status = *accessor;
                  
                  {
                     SyncLockBlock lock2( status.getReadersLock() );
                     status.deleteReader(depObj);
                  }
                        
                  if ( status.isEmpty() && !status.isOnHold( ) ) {
                     accessor.erase( );
                  }
               }
            }
            
            void removeCommDO ( CommutationDO *commDO, BaseDependency const &target )
            {
               const Region& region( dynamic_cast<const Region&>( target ) );
               
               SyncRecursiveLockBlock lock1( getInstanceLock() );
               RegionMap::iterator_list_t subregions;
               _regionMap.find( region, /* out */subregions );
               
               for (
                  RegionMap::iterator_list_t::iterator it = subregions.begin();
                  it != subregions.end();
                  it++
               ) {
                  RegionMap::iterator &accessor = *it;
                  TrackableObject &status = *accessor;
                  
                  if ( status.getCommDO ( ) == commDO ) {
                     status.setCommDO ( 0 );
                  }
                  
                  if ( status.isEmpty() && !status.isOnHold( ) ) {
                     accessor.erase( );
                  }
               }
            }

         public:
            RegionDependenciesDomain() : BaseDependenciesDomain(), _regionMap( ) {}
            RegionDependenciesDomain ( const RegionDependenciesDomain &depDomain )
               : BaseDependenciesDomain( depDomain ),
               _regionMap ( depDomain._regionMap ) {}
            
            ~RegionDependenciesDomain()
            {
            }
            
            /*!
             *  \note This function cannot be implemented in
             *  BaseDependenciesDomain since it calls a template function,
             *  and they cannot be virtual.
             */
            inline void submitDependableObject ( DependableObject &depObj, std::vector<DataAccess> &deps, SchedulePolicySuccessorFunctor* callback )
            {
               submitDependableObjectInternal ( depObj, deps.begin(), deps.end(), callback );
            }
            
            /*!
             *  \note This function cannot be implemented in
             *  BaseDependenciesDomain since it calls a template function,
             *  and they cannot be virtual.
             */
            inline void submitDependableObject ( DependableObject &depObj, size_t numDeps, DataAccess* deps, SchedulePolicySuccessorFunctor* callback )
            {
               submitDependableObjectInternal ( depObj, deps, deps+numDeps, callback );
            }
            
         
      };
      
      template void RegionDependenciesDomain::submitDependableObjectInternal ( DependableObject &depObj, const DataAccess* begin, const DataAccess* end, SchedulePolicySuccessorFunctor* callback );
      template void RegionDependenciesDomain::submitDependableObjectInternal ( DependableObject &depObj, std::vector<DataAccess>::const_iterator begin, std::vector<DataAccess>::const_iterator end, SchedulePolicySuccessorFunctor* callback );
      
      /*! \brief Default plugin implementation.
       */
      class RegionDependenciesManager : public DependenciesManager
      {
         public:
            RegionDependenciesManager() : DependenciesManager("Nanos regions dependencies domain") {}
            virtual ~RegionDependenciesManager () {}
            
            /*! \brief Creates a default dependencies domain.
             */
            DependenciesDomain* createDependenciesDomain () const
            {
               return NEW RegionDependenciesDomain();
            }
      };
  
      class RegionDepsPlugin : public Plugin
      {
            
         public:
            RegionDepsPlugin() : Plugin( "Nanos++ regions dependencies management plugin",1 )
            {
            }

            virtual void config ( Config &cfg )
            {
            }

            virtual void init()
            {
               sys.setDependenciesManager(NEW RegionDependenciesManager());
            }
      };

   }
}

DECLARE_PLUGIN("regions-default",nanos::ext::RegionDepsPlugin);