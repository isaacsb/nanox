#!/bin/sh

#HOW TO USE:
#Set EXTRAE_HOME and EXTRAE_HOME_MIC (if using MIC)
#set EXTRAE_DIR to the same folder than ./extrae.xml one (if using XML)
#Call this script before your program (for example, $NANOX_HOME/share/offload_instrumentation.sh mpirun -n 2 ./nbody.sh)

AUTO_MERGE_EXTRAE_TRACES=yes
CLEAR_EXTRAE_TMP_FILES=no
ulimit -c unlimited

export EXTRAE_HOME=${EXTRAE_HOME:-/gpfs/scratch/bsc15/bsc15250/marenostrum/extrae3.0-host}
export EXTRAE_HOME_MIC=${EXTRAE_HOME_MIC:-/gpfs/scratch/bsc15/bsc15250/marenostrum/extrae-mic}


if [ ! -f $EXTRAE_HOME_MIC/bin/mpi2prv ]
then
	echo "Warning: $EXTRAE_HOME_MIC/bin/mpi2prv not found, please configure extrae path with EXTRAE_HOME_MIC env var if you want to trace sfor mics"
fi
if [ ! -f $EXTRAE_HOME/bin/mpi2prv ]
then
	echo "ERROR: $EXTRAE_HOME/bin/mpi2prv not found, please configure extrae path with EXTRAE_HOME env var, exiting"
	exit
fi

#Enable NX_OFFLOAD_INSTRUMENTATION
if [ -z "${NATIVE_INSTRUMENTATION+xxx}" ]; 
then 
	export NX_OFFLOAD_INSTRUMENTATION=1
fi
export EXTRAE_ON=1
export TRACE_OUTPUT_DIR=./traces
export NX_INSTRUMENTATION=extrae
export EXTRAE_DIR=${EXTRAE_DIR:-$PWD}  #keep this folder with the same value than temporal-directory in extrae.xml
#export EXTRAE_CONFIG_FILE=./extrae.xml

## Run the program
if [ $# -ne 0 ]
then
#remove existing traces
rm -rf $EXTRAE_DIR/*.spawn $EXTRAE_DIR/*.mpits $EXTRAE_DIR/set-0
LD_PRELOAD=${EXTRAE_HOME}/lib/libnanosmpitrace.so:${EXTRAE_HOME_MIC}/lib/libnanosmpitrace.so $* 2>&1 | grep -v -e /lib/libnanosmpitrace.so -e "MPI_THREAD_MULTIPLE and MPI_THREAD_SERIALIZED"
fi


#If nothing launched, try merging (maybe launched just to merge) 
if [ $AUTO_MERGE_EXTRAE_TRACES == yes -o $# -eq 0 ]
then
	mkdir -p $TRACE_OUTPUT_DIR > /dev/null 2>&1
	num_files=0
	FILES=`find $EXTRAE_DIR -maxdepth 1 | grep mpits$`
	for file in $FILES;
	do
	  num_files=$((num_files+1))
	done
	EXEC_NAME="TRACE"
	for word in $*
	do
		if [[ $word == *.intel64 ]]
		then 
			EXEC_NAME=${word%.intel64}
		fi
		if [[ $word == *.mic ]]
		then 
			EXEC_NAME=${word%.mic}
		fi
	done
	TRACE_NAME=$(basename ${EXEC_NAME})
	MERGER="$EXTRAE_HOME/bin/mpi2prv"
	TMP_NAME=$EXTRAE_DIR/TRACE.mpits
	MERGER="$MERGER -f $TMP_NAME"
	TWOLINES="--"
	counter=0
	fileCounter=1
	TMP_NAME=TRACE-${counter}.mpits
	while (( $fileCounter < $num_files ));
	do
	  if [ -f $TMP_NAME ]
	  then
		  MERGER="$MERGER $TWOLINES -f $TMP_NAME"
		  fileCounter=$((fileCounter+1))
	  fi
	  counter=$((counter+1))
	  TMP_NAME=$EXTRAE_DIR/TRACE-${counter}.mpits
	done
	
	if [ ! -f $EXTRAE_DIR/TRACE.mpits ]
	then
		echo "Error, no TRACE.mpits (should have been generated by instrumentation) exists in $EXTRAE_DIR "
	else
		#Call mpi2prv Hide the errors (there are a few unmatched communications at finalization,
		#they should not matter, will be fixed in a future)
		$MERGER -o ${TRACE_OUTPUT_DIR}/${TRACE_NAME}.prv 2>&1 | grep -v -e "Error"
		if [ $CLEAR_EXTRAE_TMP_FILES == yes ]
		then
			rm -rf $EXTRAE_DIR/*.spawn $EXTRAE_DIR/*.mpits $EXTRAE_DIR/set-0
		fi
	fi
fi