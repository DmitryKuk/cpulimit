#!/bin/bash

I=0
while (( $I < 1000000000000 )); do
	echo $I
	let "++I"
done