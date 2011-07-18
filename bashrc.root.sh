#!/bin/sh

function appendpath(){
    if ! (echo $PATH | grep "$1" > /dev/null) && [ -d $1 ]
    then
	export PATH=$1:"$PATH"
    fi
}
function prependpath(){
    if ! (echo $PATH | grep "$1" > /dev/null) && [ -d $1 ]
    then
	export PATH=$1:"$PATH"
    fi
}

USER=haya

prependpath /home/$USER/$(hostname -s)/usr/bin
prependpath /home/$USER/$(hostname -s)/repos/jkr/bin
prependpath $(pwd)/root/usr/bin
export RUBYLIB=/home/haya/$(hostname -s)/repos/jkr/lib:$RUBYLIB
