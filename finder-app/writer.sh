#!/bin/bash

FILE_PATH=$1
STRING=$2

if [ $# -lt 2 ]
then
	echo "Invalid format"
	help
	exit 1
fi

echo $STRING > $FILE_PATH


help(){
	echo "writer file_path string"
}
