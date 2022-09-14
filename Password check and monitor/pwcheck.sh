#!/bin/bash

#DO NOT REMOVE THE FOLLOWING TWO LINES
git add $0 >> .local.git.out
git commit -a -m "Lab 2 commit" >> .local.git.out
git push >> .local.git.out || echo


#Your code here
FILE=$1
PASS=$(< $FILE)
LEN=${#PASS}

if [ $LEN -gt 32 ] || [ $LEN -lt 6 ]; then
  echo "Error: Password length invalid."
  exit
fi

PTS=$LEN
if /bin/egrep -q [#$\+%@] $FILE; then
  let PTS=PTS+5
fi

if /bin/egrep -q [0-9] $FILE; then
  let PTS=PTS+5
fi

if /bin/egrep -q [a-zA-Z] $FILE; then
  let PTS=PTS+5
fi

if /bin/egrep -q ".*[a-z][a-z][a-z].*" $FILE; then
  let PTS=PTS-3
fi

if /bin/egrep -q ".*[A-Z][A-Z][A-Z].*" $FILE; then
  let PTS=PTS-3
fi

if /bin/egrep -q ".*[0-9][0-9][0-9].*" $FILE; then
  let PTS=PTS-3
fi

if /bin/egrep -q "([0-9a-zA-Z])\1" $FILE; then
  let PTS=PTS-10
fi

echo "Password Score: $PTS"
