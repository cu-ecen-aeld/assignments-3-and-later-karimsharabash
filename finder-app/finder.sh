#!/bin/bash

FILES_DIR=$1
SEARCH_STR=$2

if [ $# -lt 2 ]
then
	echo "Invalid format"
	help
	exit 1
fi

if [ -d $FILES_DIR ]
then
	FILE_COUNT="find ${FILES_DIR}  -type f -follow -print | wc -l"
	FOUND_COUNT="grep ${SEARCH_STR} -r ${FILES_DIR} | wc -l "

	echo "The number of files are $(eval "$FILE_COUNT") and the number of matching lines are $(eval "$FOUND_COUNT")"	

else
	echo "${FILES_DIR} is not a driectory"
	exit 1
	
fi	


help(){
	echo "finder files_directory search_string"
}
